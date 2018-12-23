import STORE from "store";
import RENDERER from "renderer";

export default class OfflinePlaybackWebSocketEndpoint {
    constructor(serverAddr) {
        this.serverAddr = serverAddr;
        this.websocket = null;
        this.lastUpdateTimestamp = 0;
        this.lastSeqNum = -1;
        this.requestTimer = null;
        this.processTimer = null;
        this.frameData = {}; // cache frames
        this.routingTime2Path = {};
    }

    initialize(params) {
        if (params && params.id && params.map) {
            STORE.playback.setRecordId(params.id);
            STORE.playback.setMapId(params.map);
        } else {
            console.error("ERROR: missing required parameter(s)");
            return;
        }

        if (params.url) {
            this.serverUrl = `${location.protocol}//${params.url}`;
        }

        try {
            this.websocket = new WebSocket(this.serverAddr);
        } catch (error) {
            console.error("Failed to establish a connection: " + error);
            setTimeout(() => {
                this.initialize(params);
            }, 1000);
            return;
        }
        this.websocket.onopen = event => {
            this.requestGroundMeta(STORE.playback.mapId);
        };
        this.websocket.onmessage = event => {
            const message = JSON.parse(event.data);

            switch (message.type) {
                case "GroundMetadata":
                    RENDERER.updateGroundMetadata(this.serverUrl, message.data);
                    this.requestFrameCount(STORE.playback.recordId);
                    break;
                case "FrameCount":
                    STORE.playback.setNumFrames(message.data);
                    if (STORE.playback.hasNext()) {
                        this.requestSimulationWorld(STORE.playback.recordId, STORE.playback.next());
                    }
                    break;
                case "RoutePath":
                    this.routingTime2Path[message.routingTime] = message.routePath;
                    break;
                case "SimWorldUpdate":
                    this.checkMessage(message);
                    STORE.setInitializationStatus(true);

                    const world = (typeof message.world) === "string"
                        ? JSON.parse(message.world): message.world;

                    if (world.routePath) {
                        this.routingTime2Path[world.routingTime] = world.routePath;
                    } else if (!(world.routingTime in this.routingTime2Path)) {
                        // A new routing needs to be fetched from backend.
                        this.requestRoutePath(STORE.playback.recordId, world.sequenceNum);
                    }

                    if (STORE.playback.isSeeking) {
                        this.processSimWorld(world);
                    }

                    if (world.sequenceNum && !(world.sequenceNum in this.frameData)) {
                        this.frameData[world.sequenceNum] = world;
                    }

                    break;
            }
        };
        this.websocket.onclose = event => {
            console.log("WebSocket connection closed, close_code: " + event.code);
            this.initialize(params);
        };
    }

    checkMessage(message) {
        if (this.lastUpdateTimestamp !== 0
            && message.timestamp - this.lastUpdateTimestamp > 150) {
            console.log("Last sim_world_update took " +
                (message.timestamp - this.lastUpdateTimestamp) + "ms");
        }
        this.lastUpdateTimestamp = message.timestamp;
        if (this.lastSeqNum !== -1
            && message.world.sequenceNum > this.lastSeqNum + 1) {
            console.debug("Last seq: " + this.lastSeqNum +
                ". New seq: " + message.world.sequenceNum + ".");
        }
        this.lastSeqNum = message.world.sequenceNum;
    }

    startPlayback(msPerFrame) {
        clearInterval(this.requestTimer);
        this.requestTimer = setInterval(() => {
            if (this.websocket.readyState === this.websocket.OPEN && STORE.playback.initialized()) {
                this.requestSimulationWorld(STORE.playback.recordId, STORE.playback.next());

                if (!STORE.playback.hasNext()) {
                    clearInterval(this.requestTimer);
                    this.requestTimer = null;
                }
            }
        }, msPerFrame/2);

        clearInterval(this.processTimer);
        this.processTimer = setInterval(() => {
            if (STORE.playback.initialized()) {
                const frameId = STORE.playback.seekingFrame;
                if (frameId in this.frameData) {
                    this.processSimWorld(this.frameData[frameId]);
                }

                if (STORE.playback.replayComplete) {
                    clearInterval(this.processTimer);
                    this.processTimer = null;
                }
            }
        }, msPerFrame);
    }

    pausePlayback() {
        clearInterval(this.requestTimer);
        clearInterval(this.processTimer);
        this.requestTimer = null;
        this.processTimer = null;
    }

    requestGroundMeta(mapId) {
        this.websocket.send(JSON.stringify({
            type: 'RetrieveGroundMeta',
            mapId: mapId,
        }));
    }

    processSimWorld(world) {
        if (STORE.playback.shouldProcessFrame(world)) {
            if (!world.routePath) {
                world.routePath = this.routingTime2Path[world.routingTime];
            }

            STORE.updateTimestamp(world.timestamp);
            RENDERER.maybeInitializeOffest(
                world.autoDrivingCar.positionX,
                world.autoDrivingCar.positionY);
            RENDERER.updateWorld(world);
            STORE.meters.update(world);
            STORE.monitor.update(world);
            STORE.trafficSignal.update(world);
        }
    }

    requestFrameCount(recordId) {
        this.websocket.send(JSON.stringify({
            type: 'RetrieveFrameCount',
            recordId: recordId,
        }));
    }

    requestSimulationWorld(recordId, frameId) {
        if (!(frameId in this.frameData)) {
            this.websocket.send(JSON.stringify({
                type : "RequestSimulationWorld",
                recordId: recordId,
                frameId: frameId,
            }));
        } else if (STORE.playback.isSeeking) {
            this.processSimWorld(this.frameData[frameId]);
        }
    }

    requestRoutePath(recordId, frameId) {
        this.websocket.send(
            JSON.stringify({
                type: "requestRoutePath",
                recordId: recordId,
                frameId: frameId,
            })
        );
    }
}
