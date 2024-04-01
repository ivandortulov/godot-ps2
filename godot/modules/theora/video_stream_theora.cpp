/*************************************************************************/
/*  video_stream_theora.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "video_stream_theora.h"

#include "globals.h"
#include "os/os.h"

#include "thirdparty/misc/yuv2rgb.h"

int VideoStreamPlaybackTheora::buffer_data() {

	char *buffer = ogg_sync_buffer(&oy, 4096);

#ifdef THEORA_USE_THREAD_STREAMING

	int read;

	do {
		thread_sem->post();
		read = MIN(ring_buffer.data_left(), 4096);
		if (read) {
			ring_buffer.read((uint8_t *)buffer, read);
			ogg_sync_wrote(&oy, read);
		} else {
			OS::get_singleton()->delay_usec(100);
		}

	} while (read == 0);

	return read;

#else

	int bytes = file->get_buffer((uint8_t *)buffer, 4096);
	ogg_sync_wrote(&oy, bytes);
	return (bytes);

#endif
}

int VideoStreamPlaybackTheora::queue_page(ogg_page *page) {
	if (theora_p) {
		ogg_stream_pagein(&to, page);
		if (to.e_o_s)
			theora_eos = true;
	}
	if (vorbis_p) {
		ogg_stream_pagein(&vo, page);
		if (vo.e_o_s)
			vorbis_eos = true;
	}
	return 0;
}

void VideoStreamPlaybackTheora::video_write(void) {
	th_ycbcr_buffer yuv;
	th_decode_ycbcr_out(td, yuv);

	/*
	int y_offset, uv_offset;
	y_offset=(ti.pic_x&~1)+yuv[0].stride*(ti.pic_y&~1);

	{
		int pixels = size.x * size.y;
		frame_data.resize(pixels * 4);
		DVector<uint8_t>::Write w = frame_data.write();
		char* dst = (char*)w.ptr();
		int p = 0;
		for (int i=0; i<size.y; i++) {

			char *in_y  = (char *)yuv[0].data+y_offset+yuv[0].stride*i;
			char *out = dst + (int)size.x * 4 * i;
			for (int j=0;j<size.x;j++) {

				dst[p++] = in_y[j];
				dst[p++] = in_y[j];
				dst[p++] = in_y[j];
				dst[p++] = 255;
			};
		}
		format = Image::FORMAT_RGBA;
	}
	//	*/

	//*

	int pitch = 4;
	frame_data.resize(size.x * size.y * pitch);
	{
		DVector<uint8_t>::Write w = frame_data.write();
		char *dst = (char *)w.ptr();

		//uv_offset=(ti.pic_x/2)+(yuv[1].stride)*(ti.pic_y/2);

		if (px_fmt == TH_PF_444) {

			yuv444_2_rgb8888((uint8_t *)dst, (uint8_t *)yuv[0].data, (uint8_t *)yuv[1].data, (uint8_t *)yuv[2].data, size.x, size.y, yuv[0].stride, yuv[1].stride, size.x << 2, 0);

		} else if (px_fmt == TH_PF_422) {

			yuv422_2_rgb8888((uint8_t *)dst, (uint8_t *)yuv[0].data, (uint8_t *)yuv[1].data, (uint8_t *)yuv[2].data, size.x, size.y, yuv[0].stride, yuv[1].stride, size.x << 2, 0);

		} else if (px_fmt == TH_PF_420) {

			yuv420_2_rgb8888((uint8_t *)dst, (uint8_t *)yuv[0].data, (uint8_t *)yuv[2].data, (uint8_t *)yuv[1].data, size.x, size.y, yuv[0].stride, yuv[1].stride, size.x << 2, 0);
		};

		format = Image::FORMAT_RGBA;
	}

	Image img(size.x, size.y, 0, Image::FORMAT_RGBA, frame_data); //zero copy image creation

	texture->set_data(img); //zero copy send to visual server

	/*

	if (px_fmt == TH_PF_444) {

		int pitch = 3;
		frame_data.resize(size.x * size.y * pitch);
		DVector<uint8_t>::Write w = frame_data.write();
		char* dst = (char*)w.ptr();

		for(int i=0;i<size.y;i++) {

			char *in_y  = (char *)yuv[0].data+y_offset+yuv[0].stride*i;
			char *out = dst + (int)size.x * pitch * i;
			char *in_u  = (char *)yuv[1].data+uv_offset+yuv[1].stride*i;
			char *in_v  = (char *)yuv[2].data+uv_offset+yuv[2].stride*i;
			for (int j=0;j<size.x;j++) {

				out[j*3+0] = in_y[j];
				out[j*3+1] = in_u[j];
				out[j*3+2] = in_v[j];
			};
		}

		format = Image::FORMAT_YUV_444;

	} else {

		int div;
		if (px_fmt!=TH_PF_422) {
			div = 2;
		}

		bool rgba = true;
		if (rgba) {

			int pitch = 4;
			frame_data.resize(size.x * size.y * pitch);
			DVector<uint8_t>::Write w = frame_data.write();
			char* dst = (char*)w.ptr();

			uv_offset=(ti.pic_x/2)+(yuv[1].stride)*(ti.pic_y / div);
			for(int i=0;i<size.y;i++) {
				char *in_y  = (char *)yuv[0].data+y_offset+yuv[0].stride*i;
				char *in_u  = (char *)yuv[1].data+uv_offset+yuv[1].stride*(i/div);
				char *in_v  = (char *)yuv[2].data+uv_offset+yuv[2].stride*(i/div);
				uint8_t *out = (uint8_t*)dst + (int)size.x * pitch * i;
				int ofs = 0;
				for (int j=0;j<size.x;j++) {

					uint8_t y, u, v;
					y = in_y[j];
					u = in_u[j/2];
					v = in_v[j/2];

					int32_t r = Math::fast_ftoi(1.164 * (y - 16) + 1.596 * (v - 128));
					int32_t g = Math::fast_ftoi(1.164 * (y - 16) - 0.813 * (v - 128) - 0.391 * (u - 128));
					int32_t b = Math::fast_ftoi(1.164 * (y - 16) + 2.018 * (u - 128));

					out[ofs++] = CLAMP(r, 0, 255);
					out[ofs++] = CLAMP(g, 0, 255);
					out[ofs++] = CLAMP(b, 0, 255);
					out[ofs++] = 255;
				}
			}

			format = Image::FORMAT_RGBA;

		} else {

			int pitch = 2;
			frame_data.resize(size.x * size.y * pitch);
			DVector<uint8_t>::Write w = frame_data.write();
			char* dst = (char*)w.ptr();

			uv_offset=(ti.pic_x/2)+(yuv[1].stride)*(ti.pic_y / div);
			for(int i=0;i<size.y;i++) {
				char *in_y  = (char *)yuv[0].data+y_offset+yuv[0].stride*i;
				char *out = dst + (int)size.x * pitch * i;
				for (int j=0;j<size.x;j++)
					out[j*2] = in_y[j];
				char *in_u  = (char *)yuv[1].data+uv_offset+yuv[1].stride*(i/div);
				char *in_v  = (char *)yuv[2].data+uv_offset+yuv[2].stride*(i/div);
				for (int j=0;j<(int)size.x>>1;j++) {
					out[j*4+1] = in_u[j];
					out[j*4+3] = in_v[j];
				}
			}

			format = Image::FORMAT_YUV_422;
		};
	};
	//	*/

	frames_pending = 1;
}

void VideoStreamPlaybackTheora::clear() {

	if (!file)
		return;

	if (vorbis_p) {
		ogg_stream_clear(&vo);
		if (vorbis_p >= 3) {
			vorbis_block_clear(&vb);
			vorbis_dsp_clear(&vd);
		};
		vorbis_comment_clear(&vc);
		vorbis_info_clear(&vi);
		vorbis_p = 0;
	}
	if (theora_p) {
		ogg_stream_clear(&to);
		th_decode_free(td);
		th_comment_clear(&tc);
		th_info_clear(&ti);
		theora_p = 0;
	}
	ogg_sync_clear(&oy);

#ifdef THEORA_USE_THREAD_STREAMING
	thread_exit = true;
	thread_sem->post(); //just in case
	Thread::wait_to_finish(thread);
	memdelete(thread);
	thread = NULL;
	ring_buffer.clear();
#endif
	//file_name = "";

	theora_p = 0;
	vorbis_p = 0;
	videobuf_ready = 0;
	frames_pending = 0;
	videobuf_time = 0;
	theora_eos = false;
	vorbis_eos = false;

	if (file) {
		memdelete(file);
	}
	file = NULL;
	playing = false;
};

void VideoStreamPlaybackTheora::set_file(const String &p_file) {

	ERR_FAIL_COND(playing);
	ogg_packet op;
	th_setup_info *ts = NULL;

	file_name = p_file;
	if (file) {
		memdelete(file);
	}
	file = FileAccess::open(p_file, FileAccess::READ);
	ERR_FAIL_COND(!file);

#ifdef THEORA_USE_THREAD_STREAMING
	thread_exit = false;
	thread_eof = false;
	//pre-fill buffer
	int to_read = ring_buffer.space_left();
	int read = file->get_buffer(read_buffer.ptr(), to_read);
	ring_buffer.write(read_buffer.ptr(), read);

	thread = Thread::create(_streaming_thread, this);

#endif

	ogg_sync_init(&oy);

	/* init supporting Vorbis structures needed in header parsing */
	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	/* init supporting Theora structures needed in header parsing */
	th_comment_init(&tc);
	th_info_init(&ti);

	theora_eos = false;
	vorbis_eos = false;

	/* Ogg file open; parse the headers */
	/* Only interested in Vorbis/Theora streams */
	int stateflag = 0;

	int audio_track_skip = audio_track;

	while (!stateflag) {
		int ret = buffer_data();
		if (ret == 0) break;
		while (ogg_sync_pageout(&oy, &og) > 0) {
			ogg_stream_state test;

			/* is this a mandated initial header? If not, stop parsing */
			if (!ogg_page_bos(&og)) {
				/* don't leak the page; get it into the appropriate stream */
				queue_page(&og);
				stateflag = 1;
				break;
			}

			ogg_stream_init(&test, ogg_page_serialno(&og));
			ogg_stream_pagein(&test, &og);
			ogg_stream_packetout(&test, &op);

			/* identify the codec: try theora */
			if (!theora_p && th_decode_headerin(&ti, &tc, &ts, &op) >= 0) {
				/* it is theora */
				copymem(&to, &test, sizeof(test));
				theora_p = 1;
			} else if (!vorbis_p && vorbis_synthesis_headerin(&vi, &vc, &op) >= 0) {

				/* it is vorbis */
				if (audio_track_skip) {
					vorbis_info_clear(&vi);
					vorbis_comment_clear(&vc);
					ogg_stream_clear(&test);
					vorbis_info_init(&vi);
					vorbis_comment_init(&vc);

					audio_track_skip--;
				} else {
					copymem(&vo, &test, sizeof(test));
					vorbis_p = 1;
				}
			} else {
				/* whatever it is, we don't care about it */
				ogg_stream_clear(&test);
			}
		}
		/* fall through to non-bos page parsing */
	}

	/* we're expecting more header packets. */
	while ((theora_p && theora_p < 3) || (vorbis_p && vorbis_p < 3)) {
		int ret;

		/* look for further theora headers */
		while (theora_p && (theora_p < 3) && (ret = ogg_stream_packetout(&to, &op))) {
			if (ret < 0) {
				fprintf(stderr, "Error parsing Theora stream headers; "
								"corrupt stream?\n");
				clear();
				return;
			}
			if (!th_decode_headerin(&ti, &tc, &ts, &op)) {
				fprintf(stderr, "Error parsing Theora stream headers; "
								"corrupt stream?\n");
				clear();
				return;
			}
			theora_p++;
		}

		/* look for more vorbis header packets */
		while (vorbis_p && (vorbis_p < 3) && (ret = ogg_stream_packetout(&vo, &op))) {
			if (ret < 0) {
				fprintf(stderr, "Error parsing Vorbis stream headers; corrupt stream?\n");
				clear();
				return;
			}
			ret = vorbis_synthesis_headerin(&vi, &vc, &op);
			if (ret) {
				fprintf(stderr, "Error parsing Vorbis stream headers; corrupt stream?\n");
				clear();
				return;
			}
			vorbis_p++;
			if (vorbis_p == 3) break;
		}

		/* The header pages/packets will arrive before anything else we
		care about, or the stream is not obeying spec */

		if (ogg_sync_pageout(&oy, &og) > 0) {
			queue_page(&og); /* demux into the appropriate stream */
		} else {
			int ret = buffer_data(); /* someone needs more data */
			if (ret == 0) {
				fprintf(stderr, "End of file while searching for codec headers.\n");
				clear();
				return;
			}
		}
	}

	/* and now we have it all.  initialize decoders */
	if (theora_p) {
		td = th_decode_alloc(&ti, ts);
		printf("Ogg logical stream %lx is Theora %dx%d %.02f fps",
				to.serialno, ti.pic_width, ti.pic_height,
				(double)ti.fps_numerator / ti.fps_denominator);
		px_fmt = ti.pixel_fmt;
		switch (ti.pixel_fmt) {
			case TH_PF_420: printf(" 4:2:0 video\n"); break;
			case TH_PF_422: printf(" 4:2:2 video\n"); break;
			case TH_PF_444: printf(" 4:4:4 video\n"); break;
			case TH_PF_RSVD:
			default:
				printf(" video\n  (UNKNOWN Chroma sampling!)\n");
				break;
		}
		if (ti.pic_width != ti.frame_width || ti.pic_height != ti.frame_height)
			printf("  Frame content is %dx%d with offset (%d,%d).\n",
					ti.frame_width, ti.frame_height, ti.pic_x, ti.pic_y);
		th_decode_ctl(td, TH_DECCTL_GET_PPLEVEL_MAX, &pp_level_max,
				sizeof(pp_level_max));
		pp_level = pp_level_max;
		pp_level = 0;
		th_decode_ctl(td, TH_DECCTL_SET_PPLEVEL, &pp_level, sizeof(pp_level));
		pp_inc = 0;

		/*{
		int arg = 0xffff;
		th_decode_ctl(td,TH_DECCTL_SET_TELEMETRY_MBMODE,&arg,sizeof(arg));
		th_decode_ctl(td,TH_DECCTL_SET_TELEMETRY_MV,&arg,sizeof(arg));
		th_decode_ctl(td,TH_DECCTL_SET_TELEMETRY_QI,&arg,sizeof(arg));
		arg=10;
		th_decode_ctl(td,TH_DECCTL_SET_TELEMETRY_BITS,&arg,sizeof(arg));
		}*/

		int w;
		int h;
		w = (ti.pic_x + ti.frame_width + 1 & ~1) - (ti.pic_x & ~1);
		h = (ti.pic_y + ti.frame_height + 1 & ~1) - (ti.pic_y & ~1);
		size.x = w;
		size.y = h;

		texture->create(w, h, Image::FORMAT_RGBA, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);

	} else {
		/* tear down the partial theora setup */
		th_info_clear(&ti);
		th_comment_clear(&tc);
	}

	th_setup_free(ts);

	if (vorbis_p) {
		vorbis_synthesis_init(&vd, &vi);
		vorbis_block_init(&vd, &vb);
		fprintf(stderr, "Ogg logical stream %lx is Vorbis %d channel %ld Hz audio.\n",
				vo.serialno, vi.channels, vi.rate);
		//_setup(vi.channels, vi.rate);

	} else {
		/* tear down the partial vorbis setup */
		vorbis_info_clear(&vi);
		vorbis_comment_clear(&vc);
	}

	playing = false;
	buffering = true;
	time = 0;
	audio_frames_wrote = 0;
};

float VideoStreamPlaybackTheora::get_time() const {

	//print_line("total: "+itos(get_total())+" todo: "+itos(get_todo()));
	//return MAX(0,time-((get_total())/(float)vi.rate));
	return time - AudioServer::get_singleton()->get_output_delay() - delay_compensation; //-((get_total())/(float)vi.rate);
};

Ref<Texture> VideoStreamPlaybackTheora::get_texture() {

	return texture;
}

void VideoStreamPlaybackTheora::update(float p_delta) {

	if (!file)
		return;

	if (!playing || paused) {
		//printf("not playing\n");
		return;
	};

#ifdef THEORA_USE_THREAD_STREAMING
	thread_sem->post();
#endif

	//double ctime =AudioServer::get_singleton()->get_mix_time();

	//print_line("play "+rtos(p_delta));
	time += p_delta;

	if (videobuf_time > get_time()) {
		return; //no new frames need to be produced
	}

	bool frame_done = false;
	bool audio_done = !vorbis_p;

	while (!frame_done || (!audio_done && !vorbis_eos)) {
		//a frame needs to be produced

		ogg_packet op;
		bool no_theora = false;

		while (vorbis_p) {
			int ret;
			float **pcm;

			bool buffer_full = false;

			/* if there's pending, decoded audio, grab it */
			if ((ret = vorbis_synthesis_pcmout(&vd, &pcm)) > 0) {

				const int AUXBUF_LEN = 4096;
				int to_read = ret;
				int16_t aux_buffer[AUXBUF_LEN];

				while (to_read) {

					int m = MIN(AUXBUF_LEN / vi.channels, to_read);

					int count = 0;

					for (int j = 0; j < m; j++) {
						for (int i = 0; i < vi.channels; i++) {

							int val = Math::fast_ftoi(pcm[i][j] * 32767.f);
							if (val > 32767) val = 32767;
							if (val < -32768) val = -32768;
							aux_buffer[count++] = val;
						}
					}

					if (mix_callback) {
						int mixed = mix_callback(mix_udata, aux_buffer, m);
						to_read -= mixed;
						if (mixed != m) { //could mix no more
							buffer_full = true;
							break;
						}
					} else {
						to_read -= m; //just pretend we sent the audio
					}
				}

				int tr = vorbis_synthesis_read(&vd, ret - to_read);

				if (vd.granulepos >= 0) {
					//	print_line("wrote: "+itos(audio_frames_wrote)+" gpos: "+itos(vd.granulepos));
				}

				//print_line("mix audio!");

				audio_frames_wrote += ret - to_read;

				//print_line("AGP: "+itos(vd.granulepos)+" added "+itos(ret-to_read));

			} else {

				/* no pending audio; is there a pending packet to decode? */
				if (ogg_stream_packetout(&vo, &op) > 0) {
					if (vorbis_synthesis(&vb, &op) == 0) { /* test for success! */
						vorbis_synthesis_blockin(&vd, &vb);
					}
				} else { /* we need more data; break out to suck in another page */
					//printf("need moar data\n");
					break;
				};
			}

			audio_done = videobuf_time < (audio_frames_wrote / float(vi.rate));

			if (buffer_full)
				break;
		}

		while (theora_p && !frame_done) {
			/* theora is one in, one out... */
			if (ogg_stream_packetout(&to, &op) > 0) {

				if (false && pp_inc) {
					pp_level += pp_inc;
					th_decode_ctl(td, TH_DECCTL_SET_PPLEVEL, &pp_level,
							sizeof(pp_level));
					pp_inc = 0;
				}
				/*HACK: This should be set after a seek or a gap, but we might not have
				a granulepos for the first packet (we only have them for the last
				packet on a page), so we just set it as often as we get it.
				To do this right, we should back-track from the last packet on the
				page and compute the correct granulepos for the first packet after
				a seek or a gap.*/
				if (op.granulepos >= 0) {
					th_decode_ctl(td, TH_DECCTL_SET_GRANPOS, &op.granulepos,
							sizeof(op.granulepos));
				}
				ogg_int64_t videobuf_granulepos;
				if (th_decode_packetin(td, &op, &videobuf_granulepos) == 0) {
					videobuf_time = th_granule_time(td, videobuf_granulepos);

					//printf("frame time %f, play time %f, ready %i\n", (float)videobuf_time, get_time(), videobuf_ready);

					/* is it already too old to be useful?  This is only actually
					 useful cosmetically after a SIGSTOP.  Note that we have to
					 decode the frame even if we don't show it (for now) due to
					 keyframing.  Soon enough libtheora will be able to deal
					 with non-keyframe seeks.  */

					if (videobuf_time >= get_time()) {
						frame_done = true;
					} else {
						/*If we are too slow, reduce the pp level.*/
						pp_inc = pp_level > 0 ? -1 : 0;
					}
				} else {
				}

			} else {
				no_theora = true;
				break;
			}
		}

		//print_line("no theora: "+itos(no_theora)+" theora eos: "+itos(theora_eos)+" frame done "+itos(frame_done));

#ifdef THEORA_USE_THREAD_STREAMING
		if (file && thread_eof && no_theora && theora_eos && ring_buffer.data_left() == 0) {
#else
		if (file && /*!videobuf_ready && */ no_theora && theora_eos) {
#endif
			printf("video done, stopping\n");
			stop();
			return;
		};
#if 0
		if (!videobuf_ready || audio_todo > 0){
			/* no data yet for somebody.  Grab another page */

			buffer_data();
			while(ogg_sync_pageout(&oy,&og)>0){
				queue_page(&og);
			}
		}
#else

		if (!frame_done || !audio_done) {
			//what's the point of waiting for audio to grab a page?

			buffer_data();
			while (ogg_sync_pageout(&oy, &og) > 0) {
				queue_page(&og);
			}
		}
#endif
		/* If playback has begun, top audio buffer off immediately. */
		//if(stateflag) audio_write_nonblocking();

		/* are we at or past time for this video frame? */
		if (videobuf_ready && videobuf_time <= get_time()) {

			//video_write();
			//videobuf_ready=0;
		} else {
			//printf("frame at %f not ready (time %f), ready %i\n", (float)videobuf_time, get_time(), videobuf_ready);
		}

		float tdiff = videobuf_time - get_time();
		/*If we have lots of extra time, increase the post-processing level.*/
		if (tdiff > ti.fps_denominator * 0.25 / ti.fps_numerator) {
			pp_inc = pp_level < pp_level_max ? 1 : 0;
		} else if (tdiff < ti.fps_denominator * 0.05 / ti.fps_numerator) {
			pp_inc = pp_level > 0 ? -1 : 0;
		}
	}

	video_write();
};

void VideoStreamPlaybackTheora::play() {

	if (!playing)
		time = 0;
	else {
		stop();
	}

	playing = true;
	delay_compensation = Globals::get_singleton()->get("audio/video_delay_compensation_ms");
	delay_compensation /= 1000.0;
};

void VideoStreamPlaybackTheora::stop() {

	if (playing) {

		clear();
		set_file(file_name); //reset
	}
	playing = false;
	time = 0;
};

bool VideoStreamPlaybackTheora::is_playing() const {

	return playing;
};

void VideoStreamPlaybackTheora::set_paused(bool p_paused) {

	paused = p_paused;
	//pau = !p_paused;
};

bool VideoStreamPlaybackTheora::is_paused(bool p_paused) const {

	return paused;
};

void VideoStreamPlaybackTheora::set_loop(bool p_enable){

};

bool VideoStreamPlaybackTheora::has_loop() const {

	return false;
};

float VideoStreamPlaybackTheora::get_length() const {

	return 0;
};

String VideoStreamPlaybackTheora::get_stream_name() const {

	return "";
};

int VideoStreamPlaybackTheora::get_loop_count() const {

	return 0;
};

float VideoStreamPlaybackTheora::get_pos() const {

	return get_time();
};

void VideoStreamPlaybackTheora::seek_pos(float p_time){

	// no
};

void VideoStreamPlaybackTheora::set_mix_callback(AudioMixCallback p_callback, void *p_userdata) {

	mix_callback = p_callback;
	mix_udata = p_userdata;
}

int VideoStreamPlaybackTheora::get_channels() const {

	return vi.channels;
}

void VideoStreamPlaybackTheora::set_audio_track(int p_idx) {

	audio_track = p_idx;
}

int VideoStreamPlaybackTheora::get_mix_rate() const {

	return vi.rate;
}

#ifdef THEORA_USE_THREAD_STREAMING

void VideoStreamPlaybackTheora::_streaming_thread(void *ud) {

	VideoStreamPlaybackTheora *vs = (VideoStreamPlaybackTheora *)ud;

	while (!vs->thread_exit) {

		//just fill back the buffer
		if (!vs->thread_eof) {

			int to_read = vs->ring_buffer.space_left();
			if (to_read) {
				int read = vs->file->get_buffer(vs->read_buffer.ptr(), to_read);
				vs->ring_buffer.write(vs->read_buffer.ptr(), read);
				vs->thread_eof = vs->file->eof_reached();
			}
		}

		vs->thread_sem->wait();
	}
}

#endif

VideoStreamPlaybackTheora::VideoStreamPlaybackTheora() {

	file = NULL;
	theora_p = 0;
	vorbis_p = 0;
	videobuf_ready = 0;
	playing = false;
	frames_pending = 0;
	videobuf_time = 0;
	paused = false;

	buffering = false;
	texture = Ref<ImageTexture>(memnew(ImageTexture));
	mix_callback = NULL;
	mix_udata = NULL;
	audio_track = 0;
	delay_compensation = 0;
	audio_frames_wrote = 0;

#ifdef THEORA_USE_THREAD_STREAMING
	int rb_power = nearest_shift(RB_SIZE_KB * 1024);
	ring_buffer.resize(rb_power);
	read_buffer.resize(RB_SIZE_KB * 1024);
	thread_sem = Semaphore::create();
	thread = NULL;
	thread_exit = false;
	thread_eof = false;

#endif
};

VideoStreamPlaybackTheora::~VideoStreamPlaybackTheora() {

#ifdef THEORA_USE_THREAD_STREAMING

	memdelete(thread_sem);
#endif
	clear();

	if (file)
		memdelete(file);
};

RES ResourceFormatLoaderVideoStreamTheora::load(const String &p_path, const String &p_original_path, Error *r_error) {
	if (r_error)
		*r_error = ERR_FILE_CANT_OPEN;

	VideoStreamTheora *stream = memnew(VideoStreamTheora);
	stream->set_file(p_path);

	if (r_error)
		*r_error = OK;

	return Ref<VideoStreamTheora>(stream);
}

void ResourceFormatLoaderVideoStreamTheora::get_recognized_extensions(List<String> *p_extensions) const {

	p_extensions->push_back("ogm");
	p_extensions->push_back("ogv");
}
bool ResourceFormatLoaderVideoStreamTheora::handles_type(const String &p_type) const {
	return (p_type == "VideoStream" || p_type == "VideoStreamTheora");
}

String ResourceFormatLoaderVideoStreamTheora::get_resource_type(const String &p_path) const {

	String exl = p_path.extension().to_lower();
	if (exl == "ogm" || exl == "ogv")
		return "VideoStreamTheora";
	return "";
}
