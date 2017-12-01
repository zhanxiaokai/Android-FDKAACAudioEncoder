#include "audio_encoder.h"

#define LOG_TAG "AudioEncoder"

AudioEncoder::AudioEncoder() {
}

AudioEncoder::~AudioEncoder() {
}

int AudioEncoder::alloc_audio_stream(const char * codec_name) {
	AVCodec *codec;
	AVSampleFormat preferedSampleFMT = AV_SAMPLE_FMT_S16;
	int preferedChannels = audioChannels;
	int preferedSampleRate = audioSampleRate;
	audioStream = avformat_new_stream(avFormatContext, NULL);
	audioStream->id = 1;
	avCodecContext = audioStream->codec;
	avCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
	avCodecContext->sample_rate = audioSampleRate;
	if (publishBitRate > 0) {
		avCodecContext->bit_rate = publishBitRate;
	} else {
		avCodecContext->bit_rate = PUBLISH_BITE_RATE;
	}
	avCodecContext->sample_fmt = preferedSampleFMT;
	LOGI("audioChannels is %d", audioChannels);
	avCodecContext->channel_layout = preferedChannels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	avCodecContext->channels = av_get_channel_layout_nb_channels(avCodecContext->channel_layout);
	avCodecContext->profile = FF_PROFILE_AAC_LOW;
	LOGI("avCodecContext->channels is %d", avCodecContext->channels);
	avCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    /* find the MP3 encoder */
//    codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
	codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		LOGI("Couldn't find a valid audio codec");
		return -1;
	}
	avCodecContext->codec_id = codec->id;

	if (codec->sample_fmts) {
		/* check if the prefered sample format for this codec is supported.
		 * this is because, depending on the version of libav, and with the whole ffmpeg/libav fork situation,
		 * you have various implementations around. float samples in particular are not always supported.
		 */
		const enum AVSampleFormat *p = codec->sample_fmts;
		for (; *p != -1; p++) {
			if (*p == audioStream->codec->sample_fmt)
				break;
		}
		if (*p == -1) {
			LOGI("sample format incompatible with codec. Defaulting to a format known to work.........");
			/* sample format incompatible with codec. Defaulting to a format known to work */
			avCodecContext->sample_fmt = codec->sample_fmts[0];
		}
	}

	if (codec->supported_samplerates) {
		const int *p = codec->supported_samplerates;
		int best = 0;
		int best_dist = INT_MAX;
		for (; *p; p++) {
			int dist = abs(audioStream->codec->sample_rate - *p);
			if (dist < best_dist) {
				best_dist = dist;
				best = *p;
			}
		}
		/* best is the closest supported sample rate (same as selected if best_dist == 0) */
		avCodecContext->sample_rate = best;
	}
	if ( preferedChannels != avCodecContext->channels
			|| preferedSampleRate != avCodecContext->sample_rate
			|| preferedSampleFMT != avCodecContext->sample_fmt) {
		LOGI("channels is {%d, %d}", preferedChannels, audioStream->codec->channels);
		LOGI("sample_rate is {%d, %d}", preferedSampleRate, audioStream->codec->sample_rate);
		LOGI("sample_fmt is {%d, %d}", preferedSampleFMT, audioStream->codec->sample_fmt);
		LOGI("AV_SAMPLE_FMT_S16P is %d AV_SAMPLE_FMT_S16 is %d AV_SAMPLE_FMT_FLTP is %d", AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_FLTP);
		swrContext = swr_alloc_set_opts(NULL,
						av_get_default_channel_layout(avCodecContext->channels),
						(AVSampleFormat)avCodecContext->sample_fmt, avCodecContext->sample_rate,
						av_get_default_channel_layout(preferedChannels),
						preferedSampleFMT, preferedSampleRate,
						0, NULL);
		if (!swrContext || swr_init(swrContext)) {
			if (swrContext)
				swr_free(&swrContext);
			return -1;
		}
	}
	if (avcodec_open2(avCodecContext, codec, NULL) < 0) {
		LOGI("Couldn't open codec");
		return -2;
	}
	avCodecContext->time_base.num = 1;
	avCodecContext->time_base.den = avCodecContext->sample_rate;
	avCodecContext->frame_size = 1024;
	return 0;

}

int AudioEncoder::alloc_avframe() {
	int ret = 0;
	AVSampleFormat preferedSampleFMT = AV_SAMPLE_FMT_S16;
	int preferedChannels = audioChannels;
	int preferedSampleRate = audioSampleRate;
	input_frame = av_frame_alloc();
	if (!input_frame) {
		LOGI("Could not allocate audio frame\n");
		return -1;
	}
	input_frame->nb_samples = avCodecContext->frame_size;
	input_frame->format = preferedSampleFMT;
	input_frame->channel_layout = preferedChannels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	input_frame->sample_rate = preferedSampleRate;
	buffer_size = av_samples_get_buffer_size(NULL, av_get_channel_layout_nb_channels(input_frame->channel_layout),
			input_frame->nb_samples, preferedSampleFMT, 0);
	samples = (uint8_t*) av_malloc(buffer_size);
	samplesCursor = 0;
	if (!samples) {
		LOGI("Could not allocate %d bytes for samples buffer\n", buffer_size);
		return -2;
	}
	LOGI("allocate %d bytes for samples buffer\n", buffer_size);
	/* setup the data pointers in the AVFrame */
	ret = avcodec_fill_audio_frame(input_frame, av_get_channel_layout_nb_channels(input_frame->channel_layout),
			preferedSampleFMT, samples, buffer_size, 0);
	if (ret < 0) {
		LOGI("Could not setup audio frame\n");
	}
	if(swrContext) {
		if (av_sample_fmt_is_planar(avCodecContext->sample_fmt)) {
			LOGI("Codec Context SampleFormat is Planar...");
		}
		/* 分配空间 */
		convert_data = (uint8_t**)calloc(avCodecContext->channels,
			        sizeof(*convert_data));
		av_samples_alloc(convert_data, NULL,
				avCodecContext->channels, avCodecContext->frame_size,
				avCodecContext->sample_fmt, 0);
		swrBufferSize = av_samples_get_buffer_size(NULL, avCodecContext->channels, avCodecContext->frame_size, avCodecContext->sample_fmt, 0);
		swrBuffer = (uint8_t *)av_malloc(swrBufferSize);
		LOGI("After av_malloc swrBuffer");
		/* 此时data[0],data[1]分别指向frame_buf数组起始、中间地址 */
		swrFrame = av_frame_alloc();
		if (!swrFrame) {
			LOGI("Could not allocate swrFrame frame\n");
			return -1;
		}
		swrFrame->nb_samples = avCodecContext->frame_size;
		swrFrame->format = avCodecContext->sample_fmt;
		swrFrame->channel_layout = avCodecContext->channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
		swrFrame->sample_rate = avCodecContext->sample_rate;
		ret = avcodec_fill_audio_frame(swrFrame, avCodecContext->channels, avCodecContext->sample_fmt, (const uint8_t*)swrBuffer, swrBufferSize, 0);
		LOGI("After avcodec_fill_audio_frame");
		if (ret < 0) {
			LOGI("avcodec_fill_audio_frame error ");
		    return -1;
		}
	}
	return ret;
}

int AudioEncoder::init(int bitRate, int channels, int sampleRate, int bitsPerSample, const char* aacFilePath, const char * codec_name) {
	avCodecContext = NULL;
	avFormatContext = NULL;
	input_frame = NULL;
	samples = NULL;
	samplesCursor = 0;
	swrContext = NULL;
	swrFrame = NULL;
	swrBuffer = NULL;
	convert_data = NULL;
	this->isWriteHeaderSuccess = false;

	totalEncodeTimeMills = 0;
	totalSWRTimeMills = 0;

	this->publishBitRate = bitRate;
	this->audioChannels = channels;
	this->audioSampleRate = sampleRate;
	int ret;
	avcodec_register_all();
	av_register_all();

	avFormatContext = avformat_alloc_context();
	LOGI("aacFilePath is %s ", aacFilePath);
	if ((ret = avformat_alloc_output_context2(&avFormatContext, NULL, NULL, aacFilePath)) != 0) {
		LOGI("avFormatContext   alloc   failed : %s", av_err2str(ret));
		return -1;
	}

	/**
	 * decoding: set by the user before avformat_open_input().
	 * encoding: set by the user before avformat_write_header() (mainly useful for AVFMT_NOFILE formats).
	 * The callback should also be passed to avio_open2() if it's used to open the file.
	 */
	if (ret = avio_open2(&avFormatContext->pb, aacFilePath, AVIO_FLAG_WRITE, NULL, NULL)) {
		LOGI("Could not avio open fail %s", av_err2str(ret));
		return -1;
	}

	this->alloc_audio_stream(codec_name);
//	this->alloc_audio_stream("libfaac");
//	this->alloc_audio_stream("libvo_aacenc");
	av_dump_format(avFormatContext, 0, aacFilePath, 1);
	// write header
	if (avformat_write_header(avFormatContext, NULL) != 0) {
		LOGI("Could not write header\n");
		return -1;
	}
	this->isWriteHeaderSuccess = true;
	this->alloc_avframe();
	return 1;
}

int AudioEncoder::init(int bitRate, int channels, int bitsPerSample, const char* aacFilePath, const char * codec_name) {
	return init(bitRate, channels, 44100, bitsPerSample, aacFilePath, codec_name);
}

void AudioEncoder::encode(byte* buffer, int size) {
	int bufferCursor = 0;
	int bufferSize = size;
	while (bufferSize >= (buffer_size - samplesCursor)) {
		int cpySize = buffer_size - samplesCursor;
		memcpy(samples + samplesCursor, buffer + bufferCursor, cpySize);
		bufferCursor += cpySize;
		bufferSize -= cpySize;
		long long beginEncodeTimeMills = getCurrentTime();
		this->encodePacket();
		totalEncodeTimeMills += (getCurrentTime() - beginEncodeTimeMills);
		samplesCursor = 0;
	}
	if (bufferSize > 0) {
		memcpy(samples + samplesCursor, buffer + bufferCursor, bufferSize);
		samplesCursor += bufferSize;
	}
}

void AudioEncoder::encodePacket() {
//	LOGI("begin encode packet..................");
	int ret, got_output;
	AVPacket pkt;
	av_init_packet(&pkt);
	AVFrame* encode_frame;
	if(swrContext) {
		long long beginSWRTimeMills = getCurrentTime();
		swr_convert(swrContext, convert_data, avCodecContext->frame_size,
				(const uint8_t**)input_frame->data, avCodecContext->frame_size);
		int length = avCodecContext->frame_size * av_get_bytes_per_sample(avCodecContext->sample_fmt);
		for (int k = 0; k < 2; ++k) {
			for (int j = 0; j < length; ++j) {
				swrFrame->data[k][j] = convert_data[k][j];
		    }
		}
		totalSWRTimeMills += (getCurrentTime() - beginSWRTimeMills);
		encode_frame = swrFrame;
	} else {
		encode_frame = input_frame;
	}
	pkt.stream_index = 0;
	pkt.duration = (int) AV_NOPTS_VALUE;
	pkt.pts = pkt.dts = 0;
	pkt.data = samples;
	pkt.size = buffer_size;
	ret = avcodec_encode_audio2(avCodecContext, &pkt, encode_frame, &got_output);
	if (ret < 0) {
		LOGI("Error encoding audio frame\n");
		return;
	}
	if (got_output) {
		if (avCodecContext->coded_frame && avCodecContext->coded_frame->pts != AV_NOPTS_VALUE)
			pkt.pts = av_rescale_q(avCodecContext->coded_frame->pts, avCodecContext->time_base, audioStream->time_base);
		pkt.flags |= AV_PKT_FLAG_KEY;
		this->duration = pkt.pts * av_q2d(audioStream->time_base);
		//此函数负责交错地输出一个媒体包。如果调用者无法保证来自各个媒体流的包正确交错，则最好调用此函数输出媒体包，反之，可以调用av_write_frame以提高性能。
		int writeCode = av_interleaved_write_frame(avFormatContext, &pkt);
	}
	av_free_packet(&pkt);
//	LOGI("leave encode packet...");
}

void AudioEncoder::destroy() {
	LOGI("start destroy!!!");
	//这里需要判断是否删除resampler(重采样音频格式/声道/采样率等)相关的资源
	if (NULL != swrBuffer) {
		free(swrBuffer);
		swrBuffer = NULL;
		swrBufferSize = 0;
	}
	if (NULL != swrContext) {
		swr_free(&swrContext);
		swrContext = NULL;
	}
	if(convert_data) {
		av_freep(&convert_data[0]);
		free(convert_data);
	}
	if (NULL != swrFrame) {
		av_frame_free(&swrFrame);
	}
	if (NULL != samples) {
		av_freep(&samples);
	}
	if (NULL != input_frame) {
		av_frame_free(&input_frame);
	}
	if(this->isWriteHeaderSuccess) {
		avFormatContext->duration = this->duration * AV_TIME_BASE;
	    LOGI("duration is %.3f", this->duration);
	    av_write_trailer(avFormatContext);
	}
	if (NULL != avCodecContext) {
		avcodec_close(avCodecContext);
		av_free(avCodecContext);
	}
	if (NULL != avCodecContext && NULL != avFormatContext->pb) {
		avio_close(avFormatContext->pb);
	}
	LOGI("end destroy!!! totalEncodeTimeMills is %d totalSWRTimeMills is %d", totalEncodeTimeMills, totalSWRTimeMills);
}
