#ifndef AUDIO_ENCODER_H
#define AUDIO_ENCODER_H
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef UINT64_C
#define UINT64_C(value)__CONCAT(value,ULL)
#endif

extern "C" {
	#include "./3rdparty/ffmpeg/include/libavcodec/avcodec.h"
	#include "./3rdparty/ffmpeg/include/libavformat/avformat.h"
	#include "./3rdparty/ffmpeg/include/libavutil/avutil.h"
	#include "./3rdparty/ffmpeg/include/libswresample/swresample.h"
	#include "./3rdparty/ffmpeg/include/libavutil/samplefmt.h"
	#include "./3rdparty/ffmpeg/include/libavutil/common.h"
	#include "./3rdparty/ffmpeg/include/libavutil/channel_layout.h"
	#include "./3rdparty/ffmpeg/include/libavutil/opt.h"
	#include "./3rdparty/ffmpeg/include/libavutil/imgutils.h"
	#include "./3rdparty/ffmpeg/include/libavutil/mathematics.h"
};

#include "./CommonTools.h"

#ifndef PUBLISH_BITE_RATE
#define PUBLISH_BITE_RATE 64000
#endif

class AudioEncoder {
private:
	AVFormatContext *avFormatContext;
	AVCodecContext *avCodecContext;
	AVStream* audioStream;

	bool isWriteHeaderSuccess;
	double duration;

	AVFrame *input_frame;
	int buffer_size;
	uint8_t *samples;
	int samplesCursor;
	SwrContext *swrContext;
	uint8_t** convert_data;
	AVFrame* swrFrame;
	uint8_t *swrBuffer;
	int swrBufferSize;

	int publishBitRate;
	int audioChannels;
	int audioSampleRate;

	int totalSWRTimeMills;
	int totalEncodeTimeMills;

	//初始化的时候，要进行的工作
	int alloc_avframe();
	int alloc_audio_stream(const char * codec_name);
	//当够了一个frame之后就要编码一个packet
	void encodePacket();

public:
	AudioEncoder();
	virtual ~AudioEncoder();

	int init(int bitRate, int channels, int sampleRate, int bitsPerSample, const char* aacFilePath, const char * codec_name);
	int init(int bitRate, int channels, int bitsPerSample, const char* aacFilePath, const char * codec_name);
	void encode(byte* buffer, int size);
	void destroy();
};
#endif //AUDIO_ENCODER_H
