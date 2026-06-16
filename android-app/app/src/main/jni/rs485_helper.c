#include <jni.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifndef TIOCSRS485
#define TIOCSRS485 0x542F
#endif
struct serial_rs485 {
    unsigned int flags;
    unsigned int delay_rts_before_send;
    unsigned int delay_rts_after_send;
    unsigned int padding[5];
};
#define SER_RS485_ENABLED (1 << 0)

JNIEXPORT jint JNICALL Java_com_batchloader_app_ModbusListener_enableRs485(JNIEnv *env, jobject thiz, jstring jport) {
    const char *port = (*env)->GetStringUTFChars(env, jport, NULL);
    if (!port) return -1;
    int fd = open(port, O_RDWR);
    if (fd < 0) { (*env)->ReleaseStringUTFChars(env, jport, port); return -2; }
    struct serial_rs485 rs485conf;
    memset(&rs485conf, 0, sizeof(rs485conf));
    rs485conf.flags = SER_RS485_ENABLED;
    int result = ioctl(fd, TIOCSRS485, &rs485conf);
    close(fd);
    (*env)->ReleaseStringUTFChars(env, jport, port);
    return result;
}
