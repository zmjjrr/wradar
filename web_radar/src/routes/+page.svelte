<script lang="ts">
  import { onMount } from "svelte";
  import { fly, fade } from "svelte/transition";
  import { getWeaponName } from "$lib/weapons";

  let ws: WebSocket | null = null;
  let wsStatus: "connecting" | "connected" | "disconnected" = "connecting";

  let showSettings = false;
  let highlightedPlayerId: number | null = null;
  let showFovCones = true;
  let playerMarkerSize = 5;
  let fovConeLength = 60;
  let showBomb = true;
  let highlightColor = "#facc15";
  let showTerrorists = true;
  let showCounterTerrorists = true;

  const mapWidth = 1024;
  const mapHeight = 1024;

  let radarWidth = mapWidth;
  let radarHeight = mapHeight;
  let radarContainer: HTMLDivElement;
  let isFullScreen = false;
  let mapOnlyView = false;
  let shareButtonText = "🔗 Share this Match";

  let overlay = { width: 0, height: 0, top: 0, left: 0 };
  $: {
    if ((isFullScreen || mapOnlyView) && radarContainer) {
      const containerRatio =
        radarContainer.clientWidth / radarContainer.clientHeight;
      const imageRatio = mapWidth / mapHeight;

      if (containerRatio > imageRatio) {
        overlay.height = radarContainer.clientHeight;
        overlay.width = overlay.height * imageRatio;
        overlay.top = 0;
        overlay.left = (radarContainer.clientWidth - overlay.width) / 2;
      } else {
        overlay.width = radarContainer.clientWidth;
        overlay.height = overlay.width / imageRatio;
        overlay.left = 0;
        overlay.top = (radarContainer.clientHeight - overlay.height) / 2;
      }
    } else {
      overlay.width = radarWidth;
      overlay.height = radarHeight;
      overlay.top = 0;
      overlay.left = 0;
    }
  }

  interface CSWeapon {
    id: number;
  }
  interface Player {
    id: number;
    name: string;
    team: number;
    health: number;
    position: { x: number; y: number; z: number }[];
    angle: { x: number; y: number; z: number }[];
    avatar: string;
    weapons: CSWeapon[];
    is_alive: boolean;
    stats: { kills: number; assists: number; deaths: number };
  }
  interface Score {
    team: string;
    score: number;
  }
  interface CSMap {
    map_name: string;
    pos_x: number;
    pos_y: number;
    scale: number;
  }
  interface BombInfo {
    planted: boolean;
    x: number;
    y: number;
    z: number;
  }

  let players: Player[] = [];
  let scores: Score[] = [];
  let maps: CSMap[] = [];
  let bombs: BombInfo[] = [];
  let posX: number, posY: number, scale: number;
  let map = "";
  $: radarPlayers = players.filter((p) => {
    if (p.team === 2 && !showTerrorists) return false;
    if (p.team === 3 && !showCounterTerrorists) return false;
    return true;
  });
  function weaponIcon(id: number) {
    return "weapons/" + getWeaponName(id).toLocaleLowerCase() + ".svg";
  }

  function worldToRadar(
    worldX: number,
    worldY: number,
    posX: number,
    posY: number,
    scale: number,
    currentWidth: number,
    currentHeight: number,
    originalWidth: number,
    originalHeight: number
  ) {
    const px = (worldX - posX) / scale;
    const py = (posY - worldY) / scale;
    const proportionalX = px / originalWidth;
    const proportionalY = py / originalHeight;
    const finalX = proportionalX * currentWidth;
    const finalY = proportionalY * currentHeight;
    const clampedX = Math.max(0, Math.min(currentWidth, finalX));
    const clampedY = Math.max(0, Math.min(currentHeight, finalY));
    return { x: clampedX, y: clampedY };
  }

  $: terrorists = players.filter((p) => p.team === 2);
  $: counterTerrorists = players.filter((p) => p.team === 3);
  const togglePlayerHighlight = (id: number) => {
    highlightedPlayerId = highlightedPlayerId === id ? null : id;
  };
  const toggleFullScreen = () => {
    if (!document.fullscreenElement) radarContainer?.requestFullscreen();
    else document.exitFullscreen();
  };

  const toggleMapOnlyView = () => {
    mapOnlyView = !mapOnlyView;
  };

  const shareMatch = async () => {
    const shareData = {
      title: "CS Live Radar",
      text: `Watching a live match on ${map}!`,
      url: window.location.href,
    };
    try {
      if (navigator.share) {
        await navigator.share(shareData);
      } else {
        await navigator.clipboard.writeText(window.location.href);
        shareButtonText = "✅ Link Copied!";
        setTimeout(() => {
          shareButtonText = "🔗 Share this Match";
        }, 2500);
      }
    } catch (err) {
      console.error("Error sharing:", err);
      alert("Could not share or copy the link.");
    }
  };


  function connectWebSocket() {
    if (ws && ws.readyState < 2) {
      ws.close();
    }
    
    wsStatus = 'connecting';
    ws = new WebSocket("ws://192.168.137.1:8080");//127.0.0.1:8080

    ws.onopen = () => {
      wsStatus = 'connected';
    };

    ws.onclose = () => {
      wsStatus = 'disconnected';
    };

    ws.onerror = (err) => {
      console.error("WebSocket error:", err);
      wsStatus = 'disconnected';
    };

    ws.onmessage = (event) => {
      if (wsStatus !== 'connected') wsStatus = 'connected';
      const data = JSON.parse(event.data);
      if (!maps.length) return;
      players = data.players;
      scores = data.score;
      bombs = data.bomb;
      players.forEach((e) => {
        e.is_alive = e.health > 0;
      });
      let csmap = maps.find((m) => m.map_name === data["map"]);

      if (csmap) {
        posX = csmap.pos_x;
        posY = csmap.pos_y;
        scale = csmap.scale;
      } else {
        console.warn("Map not found:", data["map"]);
      }
      map = data["map"];
    };
  }

  onMount(() => {
    const handleFullScreenChange = () => {
      isFullScreen = !!document.fullscreenElement;
    };
    document.addEventListener("fullscreenchange", handleFullScreenChange);

    const fetchMapsAndConnect = async () => {
      const res = await fetch("/maps.json");
      maps = (await res.json()).maps;
      connectWebSocket();
    };
    
    fetchMapsAndConnect();

    return () => {
      document.removeEventListener("fullscreenchange", handleFullScreenChange);
      ws?.close();
    }
  });
</script>




<div
  class="bg-gradient-to-b from-slate-900 to-slate-950 text-slate-200"
  class:min-h-screen={!mapOnlyView}
  class:h-screen={mapOnlyView}
  class:p-2={!mapOnlyView}
  class:sm:p-4={!mapOnlyView}
  class:lg:p-8={!mapOnlyView}
>
  <div
    class="w-full mx-auto bg-slate-800/50"
    class:max-w-7xl={!mapOnlyView}
    class:rounded-2xl={!mapOnlyView}
    class:shadow-2xl={!mapOnlyView}
    class:p-4={!mapOnlyView}
    class:border={!mapOnlyView}
    class:border-slate-700={!mapOnlyView}
    class:h-full={mapOnlyView}
  >
    {#if !mapOnlyView}
      <div class="text-center mb-4">
        <h1 class="text-2xl font-bold uppercase tracking-widest text-slate-400">
          Current Map: <span class="text-white">{map}</span>
        </h1>
      </div>
    {/if}

    {#if wsStatus !== 'connected'}
      <div
        class="fixed inset-0 z-50 flex items-center justify-center bg-slate-950/80 backdrop-blur-sm"
        transition:fade={{ duration: 200 }}
      >
        <div
          class="bg-slate-800 border border-slate-700 rounded-2xl p-8 shadow-2xl text-center max-w-sm w-full mx-4"
          in:fly={{ y: 20, duration: 300, delay: 150 }}
          out:fade={{ duration: 150 }}
        >
          <svg
            class="w-16 h-16 mx-auto mb-4"
            class:text-red-500={wsStatus === 'disconnected'}
            class:text-sky-500={wsStatus === 'connecting'}
            class:animate-spin={wsStatus === 'connecting'}
            xmlns="http://www.w3.org/2000/svg"
            fill="none"
            viewBox="0 0 24 24"
            stroke-width="1.5"
            stroke="currentColor"
          >
            {#if wsStatus === 'connecting'}
              <path d="M256 48a208 208 0 0 1 187 116" />
  <polyline points="472 48 472 184 336 184" fill="none" stroke="black" stroke-width="40" stroke-linecap="round" stroke-linejoin="round"/>
  
  <!-- Bottom arc with arrowhead -->
  <path d="M256 464a208 208 0 0 1 -187 -116" />
  <polyline points="40 464 40 328 176 328" fill="none" stroke="black" stroke-width="40" stroke-linecap="round" stroke-linejoin="round"/>
            {:else}
              <path
                stroke-linecap="round"
                stroke-linejoin="round"
                d="M12 9v3.75m-9.303 3.376c-.866 1.5.217 3.374 1.948 3.374h14.71c1.73 0 2.813-1.874 1.948-3.374L13.949 3.378c-.866-1.5-3.032-1.5-3.898 0L2.697 16.126zM12 15.75h.007v.008H12v-.008z"
              />
            {/if}
          </svg>

          <h2 class="text-2xl font-bold text-slate-200 mb-2">
            {#if wsStatus === 'connecting'}
              Connecting
            {:else}
              Connection Lost
            {/if}
          </h2>
          <p class="text-slate-400 mb-6">
            {#if wsStatus === 'connecting'}
              Attempting to connect to the live match server...
            {:else}
              Could not establish a connection. Please check your network and try again.
            {/if}
          </p>
          {#if wsStatus === 'disconnected'}
            <button
              on:click={connectWebSocket}
              class="w-full bg-sky-500 text-white font-bold px-8 py-3 rounded-lg hover:bg-sky-600 transition-colors duration-200 shadow-lg focus:outline-none focus:ring-2 focus:ring-sky-400 focus:ring-opacity-75"
            >
              Retry
            </button>
          {/if}
        </div>
      </div>
    {/if}

    <div
      class="relative w-full rounded-lg shadow-lg overflow-hidden border-2 border-slate-700 group"
      class:aspect-square={!isFullScreen && !mapOnlyView}
      class:h-full={mapOnlyView || isFullScreen}
      class:bg-black={isFullScreen || mapOnlyView}
      bind:clientWidth={radarWidth}
      bind:clientHeight={radarHeight}
      bind:this={radarContainer}
    >
      <img
        src="/{map}_radar_psd.png"
        alt="map_img"
        class="absolute inset-0 w-full h-full z-0"
        class:object-cover={!isFullScreen && !mapOnlyView}
        class:object-contain={isFullScreen || mapOnlyView}
      />

      <div class="absolute top-2 right-2 z-30 flex gap-2">
        <button
          on:click={() => (showSettings = !showSettings)}
          class="p-2 bg-slate-900/50 rounded-full text-white opacity-0 group-hover:opacity-100 focus:opacity-100 transition-opacity duration-300 hover:bg-slate-900/80"
          aria-label="Toggle Settings"
          title="Toggle Settings"
        >
          <svg
            xmlns="http://www.w.org/2000/svg"
            class="h-6 w-6"
            class:text-sky-400={showSettings}
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            stroke-width="2"
            ><path
              stroke-linecap="round"
              stroke-linejoin="round"
              d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z"
            /><path
              stroke-linecap="round"
              stroke-linejoin="round"
              d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"
            /></svg
          >
        </button>
        <button
          on:click={toggleMapOnlyView}
          class="p-2 bg-slate-900/50 rounded-full text-white opacity-0 group-hover:opacity-100 focus:opacity-100 transition-opacity duration-300 hover:bg-slate-900/80"
          aria-label="Toggle Map-Only View"
          title="Toggle Map-Only View"
        >
          {#if !mapOnlyView}
            <svg
              xmlns="http://www.w.w3.org/2000/svg"
              class="h-6 w-6"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              stroke-width="2"
              ><path
                stroke-linecap="round"
                stroke-linejoin="round"
                d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"
              /><path
                stroke-linecap="round"
                stroke-linejoin="round"
                d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z"
              /></svg
            >
          {:else}
            <svg
              xmlns="http://www.w3.org/2000/svg"
              class="h-6 w-6"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              stroke-width="2"
              ><path
                stroke-linecap="round"
                stroke-linejoin="round"
                d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.543-7a9.97 9.97 0 011.563-3.029m5.858.908a3 3 0 114.243 4.243M9.878 9.878l4.242 4.242M9.88 9.88l-3.29-3.29m7.532 7.532l3.29 3.29M3 3l3.59 3.59m0 0A9.953 9.953 0 0112 5c4.478 0 8.268 2.943 9.543 7a10.025 10.025 0 01-4.132 5.411m0 0L21 21"
              /></svg
            >
          {/if}
        </button>
        <button
          on:click={toggleFullScreen}
          class="p-2 bg-slate-900/50 rounded-full text-white opacity-0 group-hover:opacity-100 focus:opacity-100 transition-opacity duration-300 hover:bg-slate-900/80"
          aria-label="Toggle Fullscreen"
          title="Toggle Fullscreen"
        >
          {#if !isFullScreen}
            <svg
              xmlns="http://www.w3.org/2000/svg"
              class="h-6 w-6"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              ><path
                stroke-linecap="round"
                stroke-linejoin="round"
                stroke-width="2"
                d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 1v4m0 0h-4m4 0l-5-5"
              /></svg
            >
          {:else}
            <svg
              xmlns="http://www.w3.org/2000/svg"
              class="h-6 w-6"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              ><path
                stroke-linecap="round"
                stroke-linejoin="round"
                stroke-width="2"
                d="M6 18L18 6M6 6l12 12"
              /></svg
            >
          {/if}
        </button>
      </div>

      <div
        class="absolute z-10 pointer-events-none"
        style="width:{overlay.width}px; height:{overlay.height}px; top:{overlay.top}px; left:{overlay.left}px;"
      >
        {#if bombs.length > 0 && bombs[0].planted && showBomb}
          {@const bombPos = worldToRadar(
            bombs[0].x,
            bombs[0].y,
            posX,
            posY,
            scale,
            overlay.width,
            overlay.height,
            mapWidth,
            mapHeight
          )}
          <svg
            class="absolute w-4 h-4 md:w-8 md:h-8 text-red-600 z-20 bomb-marker pointer-events-auto"
            style="left: {bombPos.x}px; top: {bombPos.y}px;"
            viewBox="0 0 35 35"
            fill="currentColor"
          >
            <path d="M9.9674 ... Z" fill="white" />
          </svg>
        {/if}

        {#each radarPlayers as player (player.id)}
          {#if player.position.length > 0 && player.angle.length > 0}
            {@const radarPos = worldToRadar(
              player.position[0].x,
              player.position[0].y,
              posX,
              posY,
              scale,
              overlay.width,
              overlay.height,
              mapWidth,
              mapHeight
            )}
            <svg
              class="absolute z-10 pointer-events-none"
              style="left:0; top:0; width:{overlay.width}px; height:{overlay.height}px;"
            >
              {#if player.id === highlightedPlayerId}
                <circle
                  cx={radarPos.x}
                  cy={radarPos.y}
                  r={playerMarkerSize * 1.2}
                  fill="none"
                  style="stroke: {highlightColor};"
                  class="highlight-ring"
                />
              {/if}

              <circle
                cx={radarPos.x}
                cy={radarPos.y}
                r={playerMarkerSize}
                fill={player.id === highlightedPlayerId
                  ? highlightColor
                  : player.team === 2
                    ? "#ef4444"
                    : "#38bdf8"}
                stroke="white"
                stroke-width="1.5"
                filter="url(#glow)"
              />

              {#if showFovCones}
                <path
                  d="M {radarPos.x} {radarPos.y}
                  L {radarPos.x +
                    Math.cos(((player.angle[0].y - 30) * Math.PI) / 180) *
                      fovConeLength}
                    {radarPos.y -
                    Math.sin(((player.angle[0].y - 30) * Math.PI) / 180) *
                      fovConeLength}
                  A {fovConeLength} {fovConeLength} 0 0 1
                    {radarPos.x +
                    Math.cos(((player.angle[0].y + 30) * Math.PI) / 180) *
                      fovConeLength}
                    {radarPos.y -
                    Math.sin(((player.angle[0].y + 30) * Math.PI) / 180) *
                      fovConeLength}
                  Z"
                  fill={player.team === 2
                    ? "url(#terroristGradient)"
                    : "url(#ctGradient)"}
                  stroke="rgba(255,255,255,0.3)"
                  stroke-width="1"
                  filter="url(#blurGlow)"
                />
              {/if}
              <defs>
                <radialGradient
                  id="terroristGradient"
                  cx="50%"
                  cy="50%"
                  r="100%"
                  ><stop offset="0%" stop-color="rgba(239,68,68,0.6)" /><stop
                    offset="100%"
                    stop-color="rgba(239,68,68,0)"
                  /></radialGradient
                >
                <radialGradient id="ctGradient" cx="50%" cy="50%" r="100%"
                  ><stop offset="0%" stop-color="rgba(56,189,248,0.6)" /><stop
                    offset="100%"
                    stop-color="rgba(56,189,248,0)"
                  /></radialGradient
                >
                <filter
                  id="blurGlow"
                  x="-50%"
                  y="-50%"
                  width="200%"
                  height="200%"
                  ><feGaussianBlur
                    in="SourceGraphic"
                    stdDeviation="3"
                    result="blur"
                  /><feMerge
                    ><feMergeNode in="blur" /><feMergeNode
                      in="SourceGraphic"
                    /></feMerge
                  ></filter
                >
                <filter id="glow" x="-50%" y="-50%" width="200%" height="200%"
                  ><feGaussianBlur
                    stdDeviation="2.5"
                    result="coloredBlur"
                  /><feMerge
                    ><feMergeNode in="coloredBlur" /><feMergeNode
                      in="SourceGraphic"
                    /></feMerge
                  ></filter
                >
              </defs>
            </svg>
          {/if}
        {/each}
      </div>
    </div>

    {#if !mapOnlyView}
      {#if showSettings}
        <div
          in:fly={{ y: -20, duration: 300 }}
          out:fade={{ duration: 200 }}
          class="my-4 bg-slate-800/60 border border-slate-700 rounded-lg p-4 shadow-lg"
        >
          <h3
            class="text-lg font-bold text-slate-400 uppercase tracking-wider mb-4 text-center"
          >
            Radar Options
          </h3>
          <div class="grid grid-cols-1 md:grid-cols-3 gap-x-8 gap-y-6">
            <div class="space-y-4">
              <h4 class="font-bold border-b border-slate-600 pb-1">
                Visibility
              </h4>
              <label class="flex items-center justify-between cursor-pointer"
                ><span class="font-semibold">Show Terrorists</span><input
                  type="checkbox"
                  bind:checked={showTerrorists}
                  class="toggle-checkbox"
                /></label
              >
              <label class="flex items-center justify-between cursor-pointer"
                ><span class="font-semibold">Show CTs</span><input
                  type="checkbox"
                  bind:checked={showCounterTerrorists}
                  class="toggle-checkbox"
                /></label
              >
              <label class="flex items-center justify-between cursor-pointer"
                ><span class="font-semibold">Show Vision Cones</span><input
                  type="checkbox"
                  bind:checked={showFovCones}
                  class="toggle-checkbox"
                /></label
              >
              <label class="flex items-center justify-between cursor-pointer"
                ><span class="font-semibold">Show Bomb Icon</span><input
                  type="checkbox"
                  bind:checked={showBomb}
                  class="toggle-checkbox"
                /></label
              >
            </div>
            <div class="space-y-4">
              <h4 class="font-bold border-b border-slate-600 pb-1">Sizing</h4>
              <label class="block">
                <div class="flex justify-between items-center">
                  <span class="font-semibold">Player Size</span><span
                    class="text-sm text-slate-400">{playerMarkerSize}</span
                  >
                </div>
                <input
                  type="range"
                  bind:value={playerMarkerSize}
                  min="2"
                  max="10"
                  step="0.5"
                  class="w-full h-2 bg-slate-700 rounded-lg appearance-none cursor-pointer range-slider"
                />
              </label>
              <label class="block">
                <div class="flex justify-between items-center">
                  <span class="font-semibold">Cone Size</span><span
                    class="text-sm text-slate-400">{fovConeLength}</span
                  >
                </div>
                <input
                  type="range"
                  bind:value={fovConeLength}
                  min="10"
                  max="150"
                  step="5"
                  class="w-full h-2 bg-slate-700 rounded-lg appearance-none cursor-pointer range-slider"
                />
              </label>
            </div>
            <div class="space-y-4">
              <h4 class="font-bold border-b border-slate-600 pb-1">
                Highlighting
              </h4>
              <label class="flex items-center justify-between">
                <span class="font-semibold">Highlight Color</span>
                <input
                  type="color"
                  bind:value={highlightColor}
                  class="w-10 h-8 p-1 bg-slate-700 border border-slate-600 rounded cursor-pointer"
                />
              </label>
              <p class="text-sm text-slate-400 italic">
                Click on a player in the lists below to highlight them on the
                map.
              </p>
            </div>
          </div>
        </div>
      {/if}

      <div class="flex flex-col md:flex-row gap-4 mt-4">
        <div class="flex-1">
          <div
            class="flex items-center justify-between bg-slate-900/70 p-2 rounded-t-lg"
          >
            <div class="flex items-center gap-3">
              <div
                class="w-8 h-8 bg-red-500 rounded-sm flex items-center justify-center font-bold"
              >
                T
              </div>
              <h2
                class="text-xl font-bold text-red-500 uppercase tracking-wider"
              >
                Terrorists
              </h2>
            </div>
            <div class="text-2xl font-bold">
              {scores.find((s) => s.team === "TERRORIST")?.score ?? 0}
            </div>
          </div>
          <div class="space-y-1.5 bg-slate-800/60 p-2 rounded-b-lg">
            {#each terrorists as player, i (player.id)}
              <div
                on:click={() => togglePlayerHighlight(player.id)}
                in:fly={{ y: 20, duration: 300, delay: i * 70 }}
                class="flex items-center bg-slate-700/50 p-2 rounded-lg transition-all duration-300 hover:bg-slate-700 hover:scale-[1.02] cursor-pointer border-2"
                class:opacity-40={!player.is_alive}
                class:saturate-0={!player.is_alive}
                class:border-yellow-400={highlightedPlayerId === player.id}
                class:border-transparent={highlightedPlayerId !== player.id}
              >
                <img
                  src={player.avatar}
                  alt={player.name}
                  class="w-11 h-11 rounded-full mr-3 border-2"
                  class:border-red-500={player.is_alive}
                  class:border-slate-600={!player.is_alive}
                />
                <div class="flex-1">
                  <p class="font-bold text-lg">{player.name}</p>
                  <div
                    class="h-1.5 w-full bg-slate-600 rounded-full mt-1 overflow-hidden"
                  >
                    <div
                      class="h-full rounded-full transition-all duration-300 ease-out health-bar"
                      style="width: {player.health}%; --health: {player.health}"
                    ></div>
                  </div>
                </div>
                <div
                  class="flex items-center justify-end text-center w-24 mx-3 font-mono"
                >
                  <span class="font-bold w-1/3 text-slate-200"
                    >{player.stats.kills}</span
                  ><span class="text-slate-400 w-1/3"
                    >{player.stats.assists}</span
                  ><span class="w-1/3 text-slate-200"
                    >{player.stats.deaths}</span
                  >
                </div>
                <div class="flex items-center justify-end space-x-2 w-40">
                  {#if player.is_alive}{#each player.weapons as weapon}<div
                        class="text-slate-200"
                      >
                        <img
                          src={weaponIcon(weapon.id)}
                          alt={getWeaponName(weapon.id)}
                          class="w-16 h-16 rounded-md"
                        />
                      </div>{/each}{/if}
                </div>
              </div>
            {/each}
          </div>
        </div>
        <div class="flex-1">
          <div
            class="flex items-center justify-between bg-slate-900/70 p-2 rounded-t-lg"
          >
            <div class="text-2xl font-bold">
              {scores.find((s) => s.team === "CT")?.score ?? 0}
            </div>
            <div class="flex items-center gap-3">
              <h2
                class="text-xl font-bold text-sky-400 uppercase tracking-wider"
              >
                Counter-Terrorists
              </h2>
              <div
                class="w-8 h-8 bg-sky-400 rounded-sm flex items-center justify-center font-bold"
              >
                CT
              </div>
            </div>
          </div>
          <div class="space-y-1.5 bg-slate-800/60 p-2 rounded-b-lg">
            {#each counterTerrorists as player, i (player.id)}
              <div
                on:click={() => togglePlayerHighlight(player.id)}
                in:fly={{ y: 20, duration: 300, delay: i * 70 }}
                class="flex items-center bg-slate-700/50 p-2 rounded-lg transition-all duration-300 hover:bg-slate-700 hover:scale-[1.02] cursor-pointer border-2"
                class:opacity-40={!player.is_alive}
                class:saturate-0={!player.is_alive}
                class:border-yellow-400={highlightedPlayerId === player.id}
                class:border-transparent={highlightedPlayerId !== player.id}
              >
                <div class="flex items-center justify-start space-x-2 w-40">
                  {#if player.is_alive}{#each player.weapons as weapon}<div
                        class="text-slate-200"
                      >
                        <img
                          src={weaponIcon(weapon.id)}
                          alt={getWeaponName(weapon.id)}
                          class="w-16 h-16 rounded-md"
                        />
                      </div>{/each}{/if}
                </div>
                <div class="flex items-center text-center w-24 mx-3 font-mono">
                  <span class="font-bold w-1/3 text-slate-200"
                    >{player.stats.kills}</span
                  ><span class="text-slate-400 w-1/3"
                    >{player.stats.assists}</span
                  ><span class="w-1/3 text-slate-200"
                    >{player.stats.deaths}</span
                  >
                </div>
                <div class="flex-1 text-right">
                  <p class="font-bold text-lg">{player.name}</p>
                  <div
                    class="h-1.5 w-full bg-slate-600 rounded-full mt-1 overflow-hidden"
                  >
                    <div
                      class="h-full rounded-full ml-auto transition-all duration-300 ease-out health-bar"
                      style="width: {player.health}%; --health: {player.health}"
                    ></div>
                  </div>
                </div>
                <img
                  src={player.avatar}
                  alt={player.name}
                  class="w-11 h-11 rounded-full ml-3 border-2"
                  class:border-sky-400={player.is_alive}
                  class:border-slate-600={!player.is_alive}
                />
              </div>
            {/each}
          </div>
        </div>
      </div>

      <div class="text-center mt-6">
        <button
          on:click={shareMatch}
          class="bg-slate-700/80 text-slate-300 px-6 py-2 rounded-lg hover:bg-slate-700 transition-colors duration-200 shadow-md"
        >
          {shareButtonText}
        </button>
      </div>
    {/if}
  </div>
</div>

<style>
  @import url("https://fonts.googleapis.com/css2?family=Roboto+Condensed:wght@400;700&display=swap");
  body {
    font-family: "Roboto Condensed", sans-serif;
  }
  .health-bar {
    --color-high: #4ade80;
    --color-mid: #facc15;
    --color-low: #ef4444;
    background-color: var(--color-low);
  }
  .health-bar[style*="--health"]:is(
      [style*="100"],
      [style*="9"],
      [style*="8"],
      [style*="7"],
      [style*="6"],
      [style*="5"]
    ) {
    background-color: var(--color-high);
  }
  .health-bar[style*="--health"]:is([style*="4"], [style*="3"], [style*="2"]) {
    background-color: var(--color-mid);
  }

  .player-marker {
    transform: translate(-50%, -50%);
    animation: pulse 2s infinite ease-in-out;
  }
  @keyframes pulse {
    0%,
    100% {
      transform: scale(1) translate(-50%, -50%);
      opacity: 1;
    }
    50% {
      transform: scale(1.2) translate(-50%, -50%);
      opacity: 0.8;
    }
  }
  .bomb-marker {
    transform: translate(-50%, -50%);
    animation: bomb-pulse 1s infinite ease-in-out;
  }
  @keyframes bomb-pulse {
    0%,
    100% {
      transform: scale(1) translate(-50%, -50%);
      filter: drop-shadow(0 0 2px #ff0000);
    }
    50% {
      transform: scale(1.1) translate(-50%, -50%);
      filter: drop-shadow(0 0 10px #ff0000);
    }
  }

  @keyframes highlight-pulse {
    0% {
      stroke-width: 3px;
      transform: scale(0.8);
      opacity: 1;
    }
    100% {
      stroke-width: 0;
      transform: scale(2.5);
      opacity: 0;
    }
  }
  .highlight-ring {
    transform-origin: center;
    animation: highlight-pulse 1.5s infinite ease-out;
    transform-box: fill-box;
  }

  .toggle-checkbox {
    appearance: none;
    width: 40px;
    height: 22px;
    background-color: #334155;
    border-radius: 9999px;
    position: relative;
    cursor: pointer;
    transition: background-color 0.2s ease-in-out;
  }
  .toggle-checkbox::before {
    content: "";
    position: absolute;
    width: 16px;
    height: 16px;
    border-radius: 50%;
    background-color: white;
    top: 3px;
    left: 3px;
    transition: transform 0.2s ease-in-out;
  }
  .toggle-checkbox:checked {
    background-color: #38bdf8;
  }
  .toggle-checkbox:checked::before {
    transform: translateX(18px);
  }

  .range-slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 18px;
    height: 18px;
    background: #f1f5f9;
    cursor: pointer;
    border-radius: 50%;
    border: 2px solid #38bdf8;
  }
  .range-slider::-moz-range-thumb {
    width: 18px;
    height: 18px;
    background: #f1f5f9;
    cursor: pointer;
    border-radius: 50%;
    border: 2px solid #38bdf8;
  }

  input[type="color"]::-webkit-color-swatch-wrapper {
    padding: 0;
  }
  input[type="color"]::-webkit-color-swatch {
    border: none;
    border-radius: 0.25rem;
  }
</style>
