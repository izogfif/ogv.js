import OGVProxyClass from './OGVProxyClass.js';

class OGVDemuxerProxy extends OGVProxyClass({
	loadedMetadata: false,
	videoFormat: null,
	frameBuffer: null,
	cpuTime: 0
}) {
	init(params, callback) {
		this.proxy('init', [params], callback);
	}

	processHeader(data, callback) {
		this.proxy('processHeader', [data], callback, [data]);
	}

	processFrame(data, callback) {
		this.proxy('processFrame', [data], callback, [data]);
	}

	close() {
		this.terminate();
	}

	sync() {
		this.proxy('sync', [], () => {});
	}

	recycleFrame(frame) {
		this.proxy('recycleFrame', [frame], () => {}, [
			frame.y.bytes.buffer,
			frame.u.bytes.buffer,
			frame.v.bytes.buffer
		]);
	}
}

export default OGVDemuxerProxy;
