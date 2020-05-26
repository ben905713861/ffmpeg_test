#include <iostream>
extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}
using namespace std;


void doError(int av_errno);

const char* inUrl = "C:/Users/wuxb/Desktop/test.flv";
const char* outUrl = "rtmp://172.16.2.148/live";
int main() {
	
	//初始化所有的封装和解封装
	av_register_all();
	
	AVFormatContext* ictx = NULL;
	int res = avformat_open_input(&ictx, inUrl, NULL, NULL);
	if(res != 0) {
		doError(res);
		return -1;
	}
	
	//获取音视频流信息
	res = avformat_find_stream_info(ictx, NULL);
	if(res != 0) {
		doError(res);
		return -1;
	}
	//打印音视频输入流信息
	av_dump_format(ictx, 0, inUrl, 0);
	
	////////////////////////////////////////////////////////
	
	//输出，创建输出流上下文
	AVFormatContext* octx = NULL;
	res =  avformat_alloc_output_context2(&octx, 0, "flv", outUrl);
	if(res < 0 || octx == NULL) {
		doError(res);
		return -2;
	}
	
	//遍历输入的avStream
	for(unsigned int i = 0; i < ictx->nb_streams; i++) {
		//在输出上下文octx中新建一个流
		//第二个参数是编码格式，这里直接取输入流的格式
		AVStream* out = avformat_new_stream(octx, ictx->streams[i]->codec->codec);
		if(out == NULL) {
			cerr << "avformat_new_stream失败" << endl;
			return -2;
		}
		//复制配置信息
//		res = avcodec_copy_context(out->codec, ictx->streams[i]->codec);
		res = avcodec_parameters_copy(out->codecpar, ictx->streams[i]->codecpar);
		out->codec->codec_tag = 0;
	}
	
	//打印音视频输出流信息
	av_dump_format(octx, 0, outUrl, 1);
	fflush(stdout);
	
	//rtmp推流
	//打开io
	res = avio_open(&octx->pb, outUrl, AVIO_FLAG_WRITE);
	if(res != 0 || octx->pb == NULL) {
		doError(res);
		return -3;
	}
	
	//写入头信息
	res = avformat_write_header(octx, NULL);
	if(res < 0) {
		doError(res);
		return -3;
	}
	cout << "avformat_write_header:" << res << endl;
	
	//推流每一帧数据
	AVPacket packet;
	//计时器，当前微秒时间戳
	long long startTime = av_gettime();
	while(true) {
		res = av_read_frame(ictx, &packet);
		//读取失败或到达结束点
		if(res < 0) {
			break;
		}
		cout << packet.pts << endl;
		
		//计算转换pts dts
		AVRational itime = ictx->streams[packet.stream_index]->time_base;
		AVRational otime = octx->streams[packet.stream_index]->time_base;
		packet.pts = av_rescale_q_rnd(packet.pts, itime, otime, (AVRounding)(AV_ROUND_INF|AV_ROUND_PASS_MINMAX));
		packet.dts = av_rescale_q_rnd(packet.dts, itime, otime, (AVRounding)(AV_ROUND_INF|AV_ROUND_PASS_MINMAX));
		packet.duration = av_rescale_q_rnd(packet.duration, itime, otime, (AVRounding)(AV_ROUND_INF|AV_ROUND_PASS_MINMAX));
		packet.pos = -1;
		
		//是视频帧，控制推送速度
		if(ictx->streams[packet.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			//时间基数
			AVRational tb = ictx->streams[packet.stream_index]->time_base;
			//运行了多久
			long long nowPassTime = av_gettime() - startTime;
			long long dts = tb.den == 0 ? 0 : packet.dts * (1000*1000* tb.num/tb.den);
			//待发送帧的时间比当前运行时间快，则休息
			if(dts > nowPassTime) {
				av_usleep(dts - nowPassTime);
			}
		}
			
		res = av_interleaved_write_frame(octx, &packet);
		if(res < 0) {
			doError(res);
			return -4;
		}
		
//		//释放帧数据
//		av_packet_unref(&packet);
	}
	
//	delete ictx;
//	delete octx;
	
	cout << "ook" << endl;
	return 0;
}

void doError(int av_errno) {
	char buff[1024];
	av_strerror(av_errno, buff, sizeof(buff));
	cerr << buff << endl;
}
