import OGVProxyClass from "./OGVProxyClass.js";

class OGVDemuxerProxy extends OGVProxyClass({
	loadedMetadata: false,
	videoFormat: null,
	frameBuffer: null,
	cpuTime: 0,
	loadedMetadata: false,
	videoCodec: null,
	audioCodec: null,
	duration: NaN,
	onseek: null,
	cpuTime: 0,
	audioPackets: [],
	audioReady: false,
	audioTimestamp: -1,
	hasVideo: false,
	hasAudio: false,
	frameReady: false,
	frameTimestamp: -1,
	keyframeTimestamp: -1,
	nextKeyframeTimestamp: -1,
	seekable: false,
	audioFormat: null,
	videoFormat: null,
}) {
	init(fileSize, file, callback) {
		this.proxy("init", [fileSize, file], callback);
		// TODO: implement properties
	}

	close() {
		this.proxy("close", [], () => { });
		this.terminate();
	}

	receiveInput(data, callback) {
		this.proxy("receiveInput", [data], () => {
			callback();
		});
	}

	process(callback) {
		this.proxy("process", [], (ret) => {
			callback(ret);
		});
	}

	dequeueAudioPacket(callback) {
		this.proxy("dequeueAudioPacket", [], (ret) => {
			callback(ret);
		});
	}

	dequeueVideoPacket(callback) {
		this.proxy("dequeueVideoPacket", [], (ret) => {
			callback(ret);
		});
	}

	flush(callback) {
		this.proxy("flush", [], () => {
			callback();
		});
	}

	getKeypointOffset(timeSeconds, callback) {
		this.proxy("getKeypointOffset", [timeSeconds], (ret) => {
			callback(ret);
		});
	}

	seekToKeypoint(timeSeconds, callback) {
		this.proxy("seekToKeypoint", [timeSeconds], (ret) => {
			callback(ret);
		});
	}
}

export default OGVDemuxerProxy;
