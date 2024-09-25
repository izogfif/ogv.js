import OGVWorkerSupport from "./OGVWorkerSupport.js";

let proxy = new OGVWorkerSupport(
	["loadedMetadata", "audioFormat", "audioBuffer", "cpuTime"],
	{
		init: function (args, callback) {
			this.target.init(...args);
			callback();
		},

		close() {
			this.target.close();
			this.terminate();
		},

		receiveInput(args, callback) {
			this.target.receiveInput(args[0]);
			callback();
		},

		process(_args, callback) {
			let ret = this.target.process();
			callback(ret);
		},

		dequeueAudioPacket(_args, callback) {
			const ret = this.target.dequeueAudioPacket();
			callback(ret);
		},

		dequeueVideoPacket(_args, callback) {
			const ret = this.target.dequeueVideoPacket();
			callback(ret);
		},

		flush(_args, callback) {
			this.target.flush();
			callback();
		},

		getKeypointOffset(args, callback) {
			const ret = this.target.getKeypointOffset(args[0]);
			callback(ret);
		},

		seekToKeypoint(args, callback) {
			const ret = this.target.seekToKeypoint(args[0]);
			callback(ret);
		},

	}
);

export default proxy;
