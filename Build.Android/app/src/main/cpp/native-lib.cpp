#include <jni.h>
#include <string>

#include "../../../../../Coroutine.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_maihd_coroutineandroidtest_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
