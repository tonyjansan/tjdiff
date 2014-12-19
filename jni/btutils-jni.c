#ifndef __JNI_NATIVE_SHARE__
#define __JNI_NATIVE_SHARE__

#include <string.h>
#include <jni.h>
#include "../btutils.h"

inline void jstringToCharArray(JNIEnv* env, char* out, jstring in)
{
	if (!in) return;
	const char* p = (*env)->GetStringUTFChars(env, in, 0);
	strcpy(out, p);
	(*env)->ReleaseStringUTFChars(env, in, p);
}

/*****************************/
/*   JNI Export Interfaces   */
/*****************************/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifdef JNI_ONLOAD
#define SHAREEXPORTAPI(rettype, fn) static rettype fn
#else
#define SHAREEXPORTAPI(rettype, fn) JNIEXPORT rettype JNICALL Java_com_lenovo_common_BTUtils_##fn
#endif // JNI_ONLOAD

SHAREEXPORTAPI(jint, BTDiff)(JNIEnv* env, jobject obj, jstring oldStr, jstring newStr, jstring diffStr)
{
	char oldfile[256], newfile[256], difffile[256];
	jstringToCharArray(env, oldfile, oldStr);
	jstringToCharArray(env, newfile, newStr);
	jstringToCharArray(env, difffile, diffStr);
	return btdiff(oldfile, newfile, difffile);
}

SHAREEXPORTAPI(jint, BTPatch)(JNIEnv* env, jobject obj, jstring oldStr, jstring newStr, jstring diffStr)
{
	char oldfile[256], newfile[256], difffile[256];
	jstringToCharArray(env, oldfile, oldStr);
	jstringToCharArray(env, newfile, newStr);
	jstringToCharArray(env, difffile, diffStr);
	return btpatch(oldfile, newfile, difffile);
}

#ifdef JNI_ONLOAD
// "com/lenovo/common/BTUtils" ^ 0x7F
static char classpath[] = { 0x1C, 0x10, 0x12, 0x50, 0x13, 0x1A, 0x11, 0x10,
                            0x09, 0x10, 0x50, 0x1C, 0x10, 0x12, 0x12, 0x10,
                            0x11, 0x50, 0x3D, 0x2B, 0x2A, 0x0B, 0x16, 0x13,
                            0x0C, 0x7F };

static JNINativeMethod methods[] = {
	{ "BTDiff", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I", (void*) BTDiff },
	{ "BTPatch", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I", (void*) BTPatch },
};

char* bufferTransform(char* buffer, char key, int length)
{
	char* origin = buffer;
	for(; buffer - origin < length; buffer++)
		*buffer ^= key;
	return origin;
}

int jniRegisterNativeMethods(JNIEnv* env, const char* className, const JNINativeMethod* gMethods, int numMethods)
{
	jclass clazz = (*env)->FindClass(env, className);
	return (clazz && (*env)->RegisterNatives(env, clazz, gMethods, numMethods) >= 0)
		? JNI_OK : JNI_ERR;
}

static int bLoaded = 0;
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env = NULL;
	jint result = JNI_ERR;
	int length = sizeof(classpath), key = 0x7F;

	if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
		return result;

	if (!bLoaded) {
		if (classpath[length - 1] != key) return result;
		bufferTransform(classpath, key, length);
		bLoaded = 1;
	}

    result = jniRegisterNativeMethods(env, classpath, methods, sizeof(methods) / sizeof(methods[0]));
    return (result == JNI_OK)? JNI_VERSION_1_4 : result;
}

#endif // JNI_ONLOAD

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  // __JNI_NATIVE_SHARE__
