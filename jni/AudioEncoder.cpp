#include "com_phuket_tour_fdkaac_encoder_AudioEncoder.h"
#include "./audio_encoder.h"

#define LOG_TAG "AudioEncoder"

void encodeAACUsingFFmpeg(const char* pcmPath, const char* aacPath, int channels, int bitRate,
		int sampleRate) {
	AudioEncoder* audioEncoder = new AudioEncoder();
	int bitsPerSample = 16;
	char * codec_name = "libfdk_aac";
	LOGI("before audioEncoder init...");
	audioEncoder->init(bitRate, channels, sampleRate, bitsPerSample, aacPath, codec_name);
	int bufferSize = 1024 * 256;
	byte* buffer = new byte[bufferSize];
	FILE* pcmFileHandle = fopen(pcmPath, "rb+");
	int readBufferSize = 0;
	while((readBufferSize = fread(buffer, 1, bufferSize, pcmFileHandle)) > 0) {
		audioEncoder->encode(buffer, readBufferSize);
	}
	LOGI("After Encode");
	delete[] buffer;
	fclose(pcmFileHandle);
	audioEncoder->destroy();
	delete audioEncoder;
}

JNIEXPORT void JNICALL Java_com_phuket_tour_fdkaac_encoder_AudioEncoder_encode
	(JNIEnv * env, jobject obj, jstring pcmPathParam, jint channels, jint bitRate,
			jint sampleRate, jstring aacPathParam) {
	const char* pcmPath = env->GetStringUTFChars(pcmPathParam, NULL);
	const char* aacPath = env->GetStringUTFChars(aacPathParam, NULL);
	encodeAACUsingFFmpeg(pcmPath, aacPath, channels, bitRate, sampleRate);
	env->ReleaseStringUTFChars(aacPathParam, aacPath);
	env->ReleaseStringUTFChars(pcmPathParam, pcmPath);
}
