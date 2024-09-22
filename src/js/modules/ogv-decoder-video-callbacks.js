/* global LibraryManager */
/* global mergeInto */
/* global Module */
/* global wasmMemory */

mergeInto(LibraryManager.library, {

	ogvjs_callback_init_video: function(frameWidth, frameHeight,
	                                    chromaWidth, chromaHeight,
                                        fps,
                                        picWidth, picHeight,
                                        picX, picY,
                                        displayWidth, displayHeight) {
		Module['videoFormat'] = {
			'width': frameWidth,
			'height': frameHeight,
			'chromaWidth': chromaWidth,
			'chromaHeight': chromaHeight,
			'cropLeft': picX,
			'cropTop': picY,
			'cropWidth': picWidth,
			'cropHeight': picHeight,
			'displayWidth': displayWidth,
			'displayHeight': displayHeight,
			'fps': fps
		};
		Module['loadedMetadata'] = true;
	},

	ogvjs_callback_frame: function(bufferY, strideY,
	                               bufferCb, strideCb,
	                               bufferCr, strideCr,
	                               width, height,
								   chromaWidth, chromaHeight,
								   picWidth, picHeight,
								   picX, picY,
								   displayWidth, displayHeight) {
	// console.log(`ogvjs_callback_frame(
	// 		strideY = ${strideY},
	// 		strideCb = ${strideCb},
	// 		strideCr = ${strideCr},
	// 		width = ${width},
	// 		height = ${height},
	// 		chromaWidth = ${chromaWidth},
	// 		chromaHeight = ${chromaHeight},
	// 		picWidth = ${picWidth},
	// 		picHeight = ${picHeight},
	// 		picX = ${picX},
	// 		picY = ${picY},
	// 		displayWidth = ${displayWidth},
	// 		displayHeight = ${displayHeight}
	// 		)`)
		// Create typed array copies of the source buffers from the emscripten heap:
		var heap = wasmMemory.buffer;
		var format = Module['videoFormat'];

		function copyAndTrim(arr, buffer, stride, height, picX, picY, picWidth, picHeight, fill) {
			arr.set(new Uint8Array(heap, buffer, stride * height));

			// Trim out anything outside the visible area
			// Protected against green stripes in some codecs (VP9)
			var x, y, ptr;
			for (ptr = 0, y = 0; y < picY; y++, ptr += stride) {
				for (x = 0; x < stride; x++) {
					arr[ptr + x] = fill;
				}
			}
			for (; y < picY + picHeight; y++, ptr += stride) {
				for (x = 0; x < picX; x++) {
					arr[ptr + x] = fill;
				}
				for (x = picX + picWidth; x < stride; x++) {
					arr[ptr + x] = fill;
				}
			}
			for (; y < height; y++, ptr += stride) {
				for (x = 0; x < stride; x++) {
					arr[ptr + x] = fill;
				}
			}
			return arr;
		}

		var outPicX = picX & ~1; // round down to divisible by 2
		var outPicY = picY & ~1; // round down to divisible by 2
		var chromaPicX = outPicX * chromaWidth / width;
		var chromaPicY = outPicY * chromaHeight / height;
		var chromaPicWidth = picWidth * chromaWidth / width;
		var chromaPicHeight = picHeight * chromaHeight / height;

		var isOriginal = (picWidth === format['cropWidth'])
					  && (picHeight === format['cropHeight']);
		if (isOriginal) {
			// This feels wrong, but in practice the WebM VP8 files I've found
			// with non-square pixels list 1920x1080 in the WebM header for
			// display size but 1440x1080 in the VP8 frame.
			//
			// Using the container-derived info to override for the original
			// resolution gets these files working, while allowing VP8 and VP9
			// files that change resolution and specify their pixels properly
			// to keep working.
			displayWidth = format['displayWidth'];
			displayHeight = format['displayHeight'];
		}

		// Try to recycle existing frame buffers.
		// This will keep from ballooning memory usage
		// between garbage collection cycles.
		var recycled = Module['recycledFrames'],
			frame,
			lenY = height * strideY,
			lenCb = chromaHeight * strideCb,
			lenCr = chromaHeight * strideCr;
		while (recycled.length > 0) {
			var next = recycled.shift(),
				format = next['format'];
			if (format['width'] === width &&
				format['height'] === height &&
				format['chromaWidth'] === chromaWidth &&
				format['chromaHeight'] === chromaHeight &&
				format['cropLeft'] === picX &&
				format['cropTop'] === picY &&
				format['cropWidth'] === picWidth &&
				format['cropHeight'] === picHeight &&
				format['displayWidth'] === displayWidth &&
				format['displayHeight'] === displayHeight &&
				next['y']['bytes'].length === lenY &&
				next['u']['bytes'].length === lenCb &&
				next['v']['bytes'].length === lenCr
			) {
				frame = next;
				break;
			}
		}
		if (!frame) {
			frame = {
				'format': {
					'width': width,
					'height': height,
					'chromaWidth': chromaWidth,
					'chromaHeight': chromaHeight,
					'cropLeft': picX,
					'cropTop': picY,
					'cropWidth': picWidth,
					'cropHeight': picHeight,
					'displayWidth': displayWidth,
					'displayHeight': displayHeight
				},
				'y': {
					'bytes': new Uint8Array(lenY),
					'stride': strideY
				},
				'u': {
					'bytes': new Uint8Array(lenCb),
					'stride': strideCb
				},
				'v': {
					'bytes': new Uint8Array(lenCr),
					'stride': strideCr
				}
			};
		}
		copyAndTrim(frame['y']['bytes'], bufferY, strideY, height, picX, picY, picWidth, picHeight, 0);
		copyAndTrim(frame['u']['bytes'], bufferCb, strideCb, chromaHeight, chromaPicX, chromaPicY, chromaPicWidth, chromaPicHeight, 128);
		copyAndTrim(frame['v']['bytes'], bufferCr, strideCr, chromaHeight, chromaPicX, chromaPicY, chromaPicWidth, chromaPicHeight, 128);

		// And queue up the output buffer!
		Module['frameBuffer'] = frame;
	},
	
	ogvjs_callback_async_complete: function(ret, cpuTime) {
		var callback = Module.callbacks.shift();
		Module['cpuTime'] += cpuTime;
		callback(ret);
		return;
	}

});
