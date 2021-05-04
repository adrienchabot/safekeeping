/* **************************************************************************
 * Copyright 2008 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **************************************************************************/

/*
 *  jDiskLib.c
 *
 *    JNI C implementation for vixDiskLib
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "jDiskLibImpl.h"
#include "vixDiskLib.h"
#include "jUtils.h"
#include "vddkFaultInjection.h"

#ifdef _WIN32
#define strdup _strdup
#endif

/*
 * Global variable for logger callbacks and declaration of logging callback
 * functions to pass down into vixMntApi.
 */
static JUtilsLogger *gLogger = NULL;
DECLARE_LOG_FUNCS(gLogger)

#ifndef _WIN32
extern void Perturb_Enable(const char *fName, int enable);
#else
void
Perturb_Enable(const char *fName, int enable)
{
   /* This is a dummy function for Windows. This will be implemented */
   /* for real later */
}
#endif

/*
 *
 * Primitives for mapping vixDiskLib data types to their corresponding
 * Java data types and vice versa. Uses access primitives to
 * manipulate individual entries.
 *
 */


/*
 *-----------------------------------------------------------------------------
 *
 * JNISetDiskGeometry --
 *
 *      Forward disk geometry data from vixDiskLib to Java.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
JNISetDiskGeometry(const VixDiskLibGeometry *geo, // IN geometry to forward
                   JNIEnv *env,                   // IN: Java Environment
                   jobject obj)                   // IN: Object
{
   JUtils_SetIntField(env, obj, "cylinders", geo->cylinders);
   JUtils_SetIntField(env, obj, "heads", geo->heads);
   JUtils_SetIntField(env, obj, "sectors", geo->sectors);
}


/*
 *-----------------------------------------------------------------------------
 *
 * JNISetDiskLibInfo --
 *
 *      Forward DiskLibInfo data from vixDiskLib to Java.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
JNISetDiskLibInfo(VixDiskLibInfo *info, // IN: disklibinfo to forward
                  JNIEnv *env,          // IN: Java Environment
                  jobject dli)          // In: Object to forward to
{
   jclass cDli;
   jobject oGeo;

   // class for disk lib info object
   cDli = (*env)->GetObjectClass(env, dli);

   // Set the bios geometry
   oGeo = JUtils_GetObjectField(env, dli, "biosGeo",
                                 "Lcom/vmware/jvix/jDiskLib$Geometry;");
   JNISetDiskGeometry(&info->biosGeo, env, oGeo);

   // Set the physical geometry
   oGeo = JUtils_GetObjectField(env, dli, "physGeo",
                                "Lcom/vmware/jvix/jDiskLib$Geometry;");
   JNISetDiskGeometry(&info->physGeo, env, oGeo);

   // Set simple data members
   JUtils_SetLongField(env, dli, "capacityInSectors", (jlong)info->capacity);
   JUtils_SetIntField(env, dli, "numLinks", (jint)info->numLinks);
   JUtils_SetIntField(env, dli, "adapterType", (jint)info->adapterType);
   JUtils_SetStringField(env, dli, "parentFileNameHint",
                         info->parentFileNameHint);
}


/*
 *-----------------------------------------------------------------------------
 *
 * JNIGetConnectParams --
 *
 *      Create a VixDiskLibConnectParams structure from a corresponding
 *      Java object.
 *
 * Results:
 *      Newly allocated VixDiskLibConnectParams object. Caller must free by
 *      calling JNIFreeConnectParams.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixDiskLibConnectParams *
JNIGetConnectParams(JNIEnv *env,  // IN: Java Environment
                    jobject conn) // IN: ConnectParams Java object
{
   jclass cls;
   VixDiskLibConnectParams *params;

   cls = (*env)->GetObjectClass(env, conn);

   params = calloc(1, sizeof *params);
   params->credType = JUtils_GetIntField(env, conn, "credType");
   params->vmxSpec = JUtils_GetStringField(env, conn, "vmxSpec");
   params->serverName = JUtils_GetStringField(env, conn, "serverName");
   params->thumbPrint = JUtils_GetStringField(env, conn, "thumbPrint");
   params->vimApiVer = JUtils_GetStringField(env, conn, "vimApiVer");
   if (params->credType == VIXDISKLIB_CRED_UID) {
      params->creds.uid.userName = JUtils_GetStringField(env, conn,
                                                         "username");
      params->creds.uid.password = JUtils_GetStringField(env, conn,
                                                         "password");
   } else if (params->credType == VIXDISKLIB_CRED_SESSIONID) {
      params->creds.sessionId.cookie = JUtils_GetStringField(env, conn,
                                                             "cookie");
      params->creds.sessionId.userName = JUtils_GetStringField(env, conn,
                                                             "username");
      params->creds.sessionId.key = JUtils_GetStringField(env, conn,
                                                             "key");
   }
   params->port = JUtils_GetIntField(env, conn, "port");
   params->nfcHostPort = JUtils_GetIntField(env, conn, "nfcHostPort");
   return params;
}


/*
 *-----------------------------------------------------------------------------
 *
 * JNIFreeConnectParams --
 *
 *      Release a VixDiskLibConnectParams object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
JNIFreeConnectParams(VixDiskLibConnectParams *params) // IN structure to free
{
   if (params == NULL) {
      return;
   }
   free(params->vmxSpec);
   free(params->serverName);
   free(params->thumbPrint);
   free(params->vimApiVer);
   if (params->credType == VIXDISKLIB_CRED_UID) {
      free(params->creds.uid.userName);
      free(params->creds.uid.password);
   } else if(params->credType == VIXDISKLIB_CRED_SESSIONID) {
      free(params->creds.sessionId.cookie);
      free(params->creds.sessionId.userName);
      free(params->creds.sessionId.key);
   }
   free(params);
}


/*
 *-----------------------------------------------------------------------------
 *
 * JNIGetCreateParams --
 *
 *      Initialize a VixDiskLibCreateParams structure from the corresponding
 *      Java object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
JNIGetCreateParams(JNIEnv *env,                     // IN: Java Environment
                   jobject params,                  // IN: Java object
                   VixDiskLibCreateParams *cParams) // OUT: C structure o init.
{
   cParams->diskType = JUtils_GetIntField(env, params, "diskType");
   cParams->adapterType = JUtils_GetIntField(env, params, "adapterType");
   cParams->hwVersion = JUtils_GetIntField(env, params, "hwVersion");
   cParams->capacity = JUtils_GetLongField(env, params, "capacityInSectors");
}


/*
 *
 * JNI Interface implementation
 *
 */


/*
 *-----------------------------------------------------------------------------
 *
 * InitJNI --
 *
 *      JNI implementation for VixDiskLib_Init. See VixDiskLib docs for
 *      details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_InitJNI(JNIEnv *env,
                                          jobject obj,
                                          jint major,
                                          jint minor,
                                          jobject logger,
                                          jstring libDir)
{
   const char *cLibDir;
   jlong result;

   gLogger = JUtils_InitLogging(env, logger);

   cLibDir = GETSTRING(libDir);

   result = VixDiskLib_Init(major, minor, &JUtils_LogFunc, &JUtils_WarnFunc,
                            &JUtils_PanicFunc, cLibDir);

   FREESTRING(cLibDir, libDir);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * InitExJNI --
 *
 *      JNI implementation for VixDiskLib_InitEx. See VixDiskLib docs for
 *      details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_InitExJNI(JNIEnv *env,
                                          jobject obj,
                                          jint major,
                                          jint minor,
                                          jobject logger,
                                          jstring libDir,
                                          jstring configFile)
{
   const char *cLibDir;
   const char *cConfigFile;
   jlong result;

   gLogger = JUtils_InitLogging(env, logger);

   cLibDir = GETSTRING(libDir);
   cConfigFile = GETSTRING(configFile);

   result = VixDiskLib_InitEx(major, minor, &JUtils_LogFunc, &JUtils_WarnFunc,
                            &JUtils_PanicFunc, cLibDir, cConfigFile);

   FREESTRING(cLibDir, libDir);
   FREESTRING(cConfigFile, configFile);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ExitJNI --
 *
 *      JNI implementation for VixDiskLib_Exit. See VixDiskLib docs for
 *      details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT void JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ExitJNI(JNIEnv *env,
                                          jobject obj)
{
   VixDiskLib_Exit();
   JUtils_ExitLogging(gLogger);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ListTransportModesJNI --
 *
 *      JNI implementation for VixDiskLib_ListTransportModes. See
 *      VixDiskLib docs for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jstring JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ListTransportModesJNI(JNIEnv *env,
                                                        jobject obj)
{
   const char *modes;

   modes = VixDiskLib_ListTransportModes();
   if (modes == NULL) {
      modes = "";
   }
   return (*env)->NewStringUTF(env, modes);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CleanupJNI --
 *
 *      JNI implementation for VixDiskLib_Cleanup. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_CleanupJNI(JNIEnv *env,
                                             jobject obj,
                                             jobject connection,
                                             jintArray aCleaned,
                                             jintArray aRemaining)
{
   uint32 numCleaned = 0, numRemaining = 0;
   VixError result;
   VixDiskLibConnectParams *params;
   jint cleaned, remaining;

   params = JNIGetConnectParams(env, connection);
   result = VixDiskLib_Cleanup(params, &numCleaned, &numRemaining);
   cleaned = numCleaned;
   remaining = numRemaining;
   (*env)->SetIntArrayRegion(env, aCleaned, 0, 1, &cleaned);
   (*env)->SetIntArrayRegion(env, aRemaining, 0, 1, &remaining);
   JNIFreeConnectParams(params);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ConnectJNI --
 *
 *      JNI implementation for VixDiskLib_Connect. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ConnectJNI(JNIEnv *env,
                                             jobject obj,
                                             jobject connection,
                                             jlongArray handle)
{
   VixDiskLibConnectParams *params;
   VixDiskLibConnection conn = NULL;
   VixError result;
   jlong jout;

   params = JNIGetConnectParams(env, connection);
   if (handle == NULL) {
      // user wants to pass NULL, go for it
      result = VixDiskLib_Connect(params, NULL);
   } else {
      result = VixDiskLib_Connect(params, &conn);
      assert(sizeof(jout) >= sizeof(conn));
      jout = (jlong)(size_t)conn;
      (*env)->SetLongArrayRegion(env, handle, 0, 1, &jout);
   }
   JNIFreeConnectParams(params);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ConnectExJNI --
 *
 *      JNI implementation for VixDiskLib_ConnectEx. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ConnectExJNI(JNIEnv *env,
                                               jobject obj,
                                               jobject connection,
                                               jboolean ro,
                                               jstring ssMoref,
                                               jstring modes,
                                               jlongArray handle)
{
   VixDiskLibConnectParams *params;
   VixDiskLibConnection conn = NULL;
   VixError result;
   jlong jout;
   const char *cssMoref, *cmodes;

   cssMoref = GETSTRING(ssMoref);
   cmodes = GETSTRING(modes);
   params = JNIGetConnectParams(env, connection);

   if (handle == NULL) {
      // user wants to pass NULL, go for it
      result = VixDiskLib_ConnectEx(params, ro, cssMoref, cmodes, NULL);
   } else {
      result = VixDiskLib_ConnectEx(params, ro, cssMoref, cmodes, &conn);
      assert(sizeof(jout) >= sizeof(conn));
      jout = (jlong)(size_t)conn;
      (*env)->SetLongArrayRegion(env, handle, 0, 1, &jout);
   }

   FREESTRING(cssMoref, ssMoref);
   FREESTRING(cmodes, modes);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DisconnectJNI --
 *
 *      JNI implementation for VixDiskLib_Disonnect. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_DisconnectJNI(JNIEnv *env,
                                                jobject obj,
                                                jlong handle)
{
   VixDiskLibConnection conn = (VixDiskLibConnection)(size_t)handle;
   return VixDiskLib_Disconnect(conn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PrepareForAccessJNI --
 *
 *      JNI implementation for VixDiskLib_PrepareForAccess. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_PrepareForAccessJNI(JNIEnv *env,
                                                      jobject obj,
                                                      jobject connection,
                                                      jstring identity)
{
   VixDiskLibConnectParams *params;
   VixError result;
   const char *cIdentity;

   cIdentity = GETSTRING(identity);
   params = JNIGetConnectParams(env, connection);

   result = VixDiskLib_PrepareForAccess(params, cIdentity);

   FREESTRING(cIdentity, identity);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * EndAccessJNI --
 *
 *      JNI implementation for VixDiskLib_EndAccess. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_EndAccessJNI(JNIEnv *env,
                                               jobject obj,
                                               jobject connection,
                                               jstring identity)
{
   VixDiskLibConnectParams *params;
   VixError result;
   const char *cIdentity;

   cIdentity = GETSTRING(identity);
   params = JNIGetConnectParams(env, connection);

   result = VixDiskLib_EndAccess(params, cIdentity);

   FREESTRING(cIdentity, identity);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OpenJNI --
 *
 *      JNI implementation for VixDiskLib_Open. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_OpenJNI(JNIEnv *env,
                                          jobject obj,
                                          jlong handle,
                                          jstring path,
                                          jint flags,
                                          jlongArray diskHandle)
{
   VixDiskLibConnection conn = (VixDiskLibConnection)(size_t)handle;
   const char *cPath;
   VixError result;
   VixDiskLibHandle cDiskHandle = NULL;
   jlong jout;

   cPath = GETSTRING(path);

   if (diskHandle == NULL) {
      // User wants to pass NULL, for great justice. Let them.
      result = VixDiskLib_Open(conn, cPath, flags, (VixDiskLibHandle*) NULL);
   } else {
      result = VixDiskLib_Open(conn, cPath, flags, &cDiskHandle);
      jout = (jlong)(size_t)cDiskHandle;
      (*env)->SetLongArrayRegion(env, diskHandle, 0, 1, &jout);
   }

   FREESTRING(cPath, path);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CloseJNI --
 *
 *      JNI implementation for VixDiskLib_Close. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_CloseJNI(JNIEnv *env,
                                           jobject obj,
                                           jlong diskHandle)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   return VixDiskLib_Close(cDiskHandle);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnlinkJNI --
 *
 *      JNI implementation for VixDiskLib_Unlink. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_UnlinkJNI(JNIEnv *env,
                                            jobject obj,
                                            jlong connHandle,
                                            jstring path)
{
   VixDiskLibConnection conn = (VixDiskLibConnection)(size_t)connHandle;
   const char *cPath;
   VixError result;

   cPath = GETSTRING(path);
   result = VixDiskLib_Unlink(conn, cPath);
   FREESTRING(cPath, path);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateJNI --
 *
 *      JNI implementation for VixDiskLib_Create. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_CreateJNI(JNIEnv *env,
                                            jobject obj,
                                            jlong connHandle,
                                            jstring path,
                                            jobject createParams,
                                            jobject progress)
{
   VixDiskLibConnection conn = (VixDiskLibConnection)(size_t)connHandle;
   const char *cPath;
   VixDiskLibCreateParams cParams;
   VixError result;

   cPath = GETSTRING(path);
   JNIGetCreateParams(env, createParams, &cParams);

   result = VixDiskLib_Create(conn, cPath, &cParams, &JUtils_ProgressFunc, progress);

   FREESTRING(cPath, path);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateChildJNI --
 *
 *      JNI implementation for VixDiskLib_CreateChild. See VixDiskLib
 *      docs for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_CreateChildJNI(JNIEnv *env,
                                                 jobject obj,
                                                 jlong diskHandle,
                                                 jstring childPath,
                                                 jint diskType,
                                                 jobject progress)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   const char *cChildPath;
   VixError result;

   cChildPath =  GETSTRING(childPath);

   result = VixDiskLib_CreateChild(cDiskHandle, cChildPath, diskType,
				   &JUtils_ProgressFunc, progress);

   FREESTRING(cChildPath, childPath);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CloneJNI --
 *
 *      JNI implementation for VixDiskLib_Clone. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_CloneJNI(JNIEnv *env,
                                           jobject obj,
                                           jlong dstConn,
                                           jstring dstPath,
                                           jlong srcConn,
                                           jstring srcPath,
                                           jobject createParams,
                                           jobject progress,
                                           jboolean overwrite)
{
   VixDiskLibConnection cDstConn = (VixDiskLibConnection)(size_t)dstConn;
   VixDiskLibConnection cSrcConn = (VixDiskLibConnection)(size_t)srcConn;
   const char *cDstPath;
   const char *cSrcPath;
   VixDiskLibCreateParams cParams;
   VixError result;

   cDstPath = GETSTRING(dstPath);
   cSrcPath = GETSTRING(srcPath);

   JNIGetCreateParams(env, createParams, &cParams);
   result = VixDiskLib_Clone(cDstConn, cDstPath, cSrcConn, cSrcPath, &cParams,
			     &JUtils_ProgressFunc, progress, overwrite);

   FREESTRING(cDstPath, dstPath);
   FREESTRING(cSrcPath, srcPath);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GrowJNI --
 *
 *      JNI implementation for VixDiskLib_Grow. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_GrowJNI(JNIEnv *env,
                                          jobject obj,
                                          jlong connHandle,
                                          jstring path,
                                          jlong capacityInSectors,
                                          jboolean updateGeometry,
                                          jobject progress)
{
   VixDiskLibConnection conn = (VixDiskLibConnection)(size_t)connHandle;
   const char *cPath;
   VixError result;

   cPath = GETSTRING(path);

   result = VixDiskLib_Grow(conn, cPath, capacityInSectors, updateGeometry,
			    &JUtils_ProgressFunc, progress);

   FREESTRING(cPath, path);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkJNI --
 *
 *      JNI implementation for VixDiskLib_Shrink. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ShrinkJNI(JNIEnv *env,
                                            jobject obj,
                                            jlong diskHandle,
                                            jobject progress)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;

   return VixDiskLib_Shrink(cDiskHandle, &JUtils_ProgressFunc, progress);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DefragmentJNI --
 *
 *      JNI implementation for VixDiskLib_Defragment. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_DefragmentJNI(JNIEnv *env,
                                                jobject obj,
                                                jlong diskHandle,
                                                jobject progress)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;

   return VixDiskLib_Defragment(cDiskHandle, &JUtils_ProgressFunc, progress);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AttachJNI --
 *
 *      JNI implementation for VixDiskLib_Attach. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_AttachJNI(JNIEnv *env,
                                            jobject obj,
                                            jlong parent,
                                            jlong child)
{
   VixDiskLibHandle cParent = (VixDiskLibHandle)(size_t)parent;
   VixDiskLibHandle cChild = (VixDiskLibHandle)(size_t)child;

   return VixDiskLib_Attach(cParent, cChild);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ReadJNI --
 *
 *      JNI implementation for VixDiskLib_Read. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ReadJNI(JNIEnv *env,
                                          jobject obj,
                                          jlong diskHandle,
                                          jlong startSector,
                                          jbyteArray buf)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   jbyte *jBuf;
   jsize bufSize;
   VixError result;

   bufSize = (*env)->GetArrayLength(env, buf);
   if (bufSize % VIXDISKLIB_SECTOR_SIZE != 0) {
      JUtils_Log("VixDiskLib buffers must be multiples of %d in "
                 "size.\n", VIXDISKLIB_SECTOR_SIZE);
      return VIX_E_INVALID_ARG;
   }
   jBuf = (*env)->GetByteArrayElements(env, buf, NULL);

   result = VixDiskLib_Read(cDiskHandle, startSector,
			    bufSize / VIXDISKLIB_SECTOR_SIZE, (uint8*)jBuf);
   (*env)->ReleaseByteArrayElements(env, buf, jBuf, 0);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ReadAsyncJNI --
 *
 *      JNI implementation for VixDiskLib_ReadAsync. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ReadAsyncJNI(JNIEnv *env,
                                               jobject obj,
                                               jlong diskHandle,
                                               jlong startSector,
                                               jobject buffer,
                                               jint sectorCount,
                                               jobject callbackObj)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   jUtilsAsyncCallback *asyncCallback = NULL;
   VixDiskLibCompletionCB completionCB = NULL;
   void *data = NULL;
   VixError result;

   if (callbackObj) {
      asyncCallback = jUtils_CreateAsyncCallback(env, callbackObj);
      completionCB = (VixDiskLibCompletionCB)jUtilsCompletionCB;
   }

   if (buffer) {
      data = (*env)->GetDirectBufferAddress(env, buffer);
   }

   result = VixDiskLib_ReadAsync(cDiskHandle,
                                 startSector,
                                 sectorCount,
                                 (uint8*)data,
                                 completionCB,
                                 (void*)asyncCallback);

   return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * WriteJNI --
 *
 *      JNI implementation for VixDiskLib_Write. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_WriteJNI(JNIEnv *env,
                                           jobject obj,
                                           jlong diskHandle,
                                           jlong startSector,
                                           jbyteArray buf)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   jbyte *jBuf;
   jsize bufSize;
   VixError result;

   bufSize = (*env)->GetArrayLength(env, buf);
   if (bufSize % VIXDISKLIB_SECTOR_SIZE != 0) {
      JUtils_Log("JNI: VixDiskLib buffers must be multiples of %d in "
                 "size.\n", VIXDISKLIB_SECTOR_SIZE);
      return VIX_E_INVALID_ARG;
   }
   jBuf = (*env)->GetByteArrayElements(env, buf, NULL);

   result = VixDiskLib_Write(cDiskHandle, startSector,
			     bufSize / VIXDISKLIB_SECTOR_SIZE, (uint8*)jBuf);

   (*env)->ReleaseByteArrayElements(env, buf, jBuf, JNI_ABORT);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WriteAsyncJNI --
 *
 *      JNI implementation for VixDiskLib_WriteAsync. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_WriteAsyncJNI(JNIEnv *env,
                                                jobject obj,
                                                jlong diskHandle,
                                                jlong startSector,
                                                jobject buffer,
                                                jint sectorCount,
                                                jobject callbackObj)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   jUtilsAsyncCallback *asyncCallback = NULL;
   VixDiskLibCompletionCB completionCB = NULL;
   void *data = NULL;
   VixError result;

   if (callbackObj) {
      asyncCallback = jUtils_CreateAsyncCallback(env, callbackObj);
      completionCB = (VixDiskLibCompletionCB)jUtilsCompletionCB;
   }

   if (buffer) {
      data = (*env)->GetDirectBufferAddress(env, buffer);
   }

   result = VixDiskLib_WriteAsync(cDiskHandle,
                                  startSector,
                                  sectorCount,
                                  (uint8*)data,
                                  completionCB,
                                  (void*)asyncCallback);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WaitJNI --
 *
 *      JNI implementation for VixDiskLib_Flush. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_WaitJNI(JNIEnv *env,
                                           jobject obj,
                                           jlong diskHandle)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   return VixDiskLib_Wait(cDiskHandle);
}

/*
 *-----------------------------------------------------------------------------
 *
 * FlushJNI --
 *
 *      JNI implementation for VixDiskLib_Flush. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_FlushJNI(JNIEnv *env,
                                           jobject obj,
                                           jlong diskHandle)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   return VixDiskLib_Flush(cDiskHandle);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetMetadataKeysJNI --
 *
 *      JNI implementation for VixDiskLib_GetMetadataKeys. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jobjectArray JNICALL
Java_com_vmware_jvix_jDiskLibImpl_GetMetadataKeysJNI(JNIEnv *env,
                                                     jobject obj,
                                                     jlong diskHandle)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   size_t required;
   VixError err;
   jobjectArray result = NULL;
   char *keys, *hlp;
   int i;
   jclass strClass;

   strClass =  (*env)->FindClass(env, "java/lang/String");

   err = VixDiskLib_GetMetadataKeys(cDiskHandle, NULL, 0, &required);
   if (err != VIX_E_BUFFER_TOOSMALL) {
      JUtils_Log("GetMetaDataKeys: Not finding any keys!\n");
      result = (*env)->NewObjectArray(env, 0, strClass, 0);
      goto out;
   }

   keys = malloc(required);
   assert(keys != NULL);
   err = VixDiskLib_GetMetadataKeys(cDiskHandle, keys, required, NULL);
   if (err != VIX_OK) {
      JUtils_Log("GetMetaDataKeys: Cannot fetch keys!\n");
      result = (*env)->NewObjectArray(env, 0, strClass, 0);
      goto out;
   }

   /* Count metadata keys */
   hlp = keys;
   i = 0;
   while (hlp[0] != '\0') {
      i += 1;
      hlp += strlen(hlp) + 1;
   }

   /* Create object array large enough to hold all strings */
   result = (*env)->NewObjectArray(env, i, strClass, 0);

   /* Fill object array. */
   hlp = keys;
   i = 0;
   while (hlp[0] != '\0') {
      jstring str;
      str = (*env)->NewStringUTF(env, hlp);
      (*env)->SetObjectArrayElement(env, result, i, str);
      i += 1;
      hlp += strlen(hlp) + 1;
   }
   free(keys);

 out:
   (*env)->DeleteLocalRef(env, strClass);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  ReadMetadataJNI --
 *
 *      JNI implementation for VixDiskLib_ReadMetadata. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_ReadMetadataJNI(JNIEnv *env,
	jobject obj,
	jlong diskHandle,
	jstring key,
	jobject valOut)
{
	VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
	const char *cKey;
	size_t required;
	VixError err;
	char *val = NULL;
	jstring result;
	jclass cls;
	jmethodID appendMid;

	cKey = GETSTRING(key);
	err = VixDiskLib_ReadMetadata(cDiskHandle, cKey, NULL, 0, &required);
	if (err != VIX_E_BUFFER_TOOSMALL) {
		JUtils_Log("ReadMetadataEx: Cannot get meta for key %s, err (%d).\n",
			cKey, err);
		goto out;
	}

	val = malloc(required);
	assert(val != NULL);

	err = VixDiskLib_ReadMetadata(cDiskHandle, cKey, val, required, NULL);
	if (err != VIX_OK) {
		JUtils_Log("ReadMetadataEx: Cannot get meta for key %s, err (%d).\n",
			cKey, err);
		goto out;
	}

	result = (*env)->NewStringUTF(env, val);
	cls = (*env)->GetObjectClass(env, valOut);
	appendMid = (*env)->GetMethodID(env, cls, "append",
		"(Ljava/lang/String;)Ljava/lang/StringBuffer;");
	if (appendMid) {
		(*env)->CallObjectMethod(env, valOut, appendMid, result);
	}

out:
	FREESTRING(cKey, key);
	free(val);
	return err;
}
 

/*
 *-----------------------------------------------------------------------------
 *
 *  WriteMetadataJNI --
 *
 *      JNI implementation for VixDiskLib_WriteMetadata. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_WriteMetadataJNI(JNIEnv *env,
                                                   jobject obj,
                                                   jlong diskHandle,
                                                   jstring key,
                                                   jstring val)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   const char *cKey;
   const char *cVal;
   VixError result;

   cKey = GETSTRING(key);
   cVal = GETSTRING(val);

   result = VixDiskLib_WriteMetadata(cDiskHandle, cKey, cVal);

   FREESTRING(cKey, key);
   FREESTRING(cVal, val);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  GetInfoJNI --
 *
 *      JNI implementation for VixDiskLib_GetInfo. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_GetInfoJNI(JNIEnv *env,
                                             jobject obj,
                                             jlong diskHandle,
                                             jobject dli)
{
   VixDiskLibInfo *info = NULL;
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   VixError result;

   if (dli == NULL) {
      result = VixDiskLib_GetInfo(cDiskHandle, NULL);
   } else {
      result = VixDiskLib_GetInfo(cDiskHandle, &info);
   }

   if (info != NULL) {
      JNISetDiskLibInfo(info, env, dli);
      VixDiskLib_FreeInfo(info);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  GetTransportModeJNI --
 *
 *      JNI implementation for VixDiskLib_GetTransportMode. See
 *      VixDiskLib docs for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jstring JNICALL
Java_com_vmware_jvix_jDiskLibImpl_GetTransportModeJNI(JNIEnv *env,
                                                      jobject obj,
                                                      jlong diskHandle)
{
   VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
   const char *mode;

   mode = VixDiskLib_GetTransportMode(cDiskHandle);
   return (*env)->NewStringUTF(env, mode);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  GetErrorTextJNI --
 *
 *      JNI implementation for VixDiskLib_GetErrorText. See VixDiskLib
 *      docs for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jstring JNICALL
Java_com_vmware_jvix_jDiskLibImpl_GetErrorTextJNI(JNIEnv *env,
                                                  jobject obj,
                                                  jlong error,
                                                  jstring locale)
{
   char *errTxt;
   const char *cLocale;
   jstring result;

   cLocale =  GETSTRING(locale);

   errTxt = VixDiskLib_GetErrorText(error, cLocale);
   if (errTxt != NULL) {
      result = (*env)->NewStringUTF(env, errTxt);
      VixDiskLib_FreeErrorText(errTxt);
   } else {
      result = (*env)->NewStringUTF(env, "");
   }

   FREESTRING(cLocale, locale);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  RenameJNI --
 *
 *      JNI implementation for VixDiskLib_Rename. See VixDiskLib docs
 *      for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_RenameJNI(JNIEnv *env,
                                            jobject obj,
                                            jstring src,
                                            jstring dst)
{
   const char *cSrc;
   const char *cDst;
   VixError result;

   cSrc = GETSTRING(src);
   cDst = GETSTRING(dst);

   result = VixDiskLib_Rename(cSrc, cDst);

   FREESTRING(cSrc, src);
   FREESTRING(cDst, dst);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  SpaceNeededForCloneJNI --
 *
 *      JNI implementation for VixDiskLib_SpaceNeedeForClone. See
 *      VixDiskLib docs for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_SpaceNeededForCloneJNI(JNIEnv *env,
                                                         jobject obj,
                                                         jlong diskHandle,
                                                         jint diskType,
                                                         jlongArray needed)
{
    VixDiskLibHandle cDiskHandle = (VixDiskLibHandle)(size_t)diskHandle;
    uint64 spaceNeeded;
    jlong jout = 0;
    VixError result;

    if (needed == NULL) {
       result = VixDiskLib_SpaceNeededForClone(cDiskHandle, diskType, NULL);
    } else {
       result = VixDiskLib_SpaceNeededForClone(cDiskHandle, diskType, &spaceNeeded);
       if (result == VIX_OK) {
          jout = (jlong)spaceNeeded;
       }

      (*env)->SetLongArrayRegion(env, needed, 0, 1, &jout);
    }
    return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CheckRepairJNI --
 *
 *      JNI implementation for VixDiskLib_CheckRepair. See
 *      VixDiskLib docs for details.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_CheckRepairJNI(JNIEnv *env,
                                                 jobject obj,
                                                 jlong connHandle,
                                                 jstring path,
                                                 jboolean repair)
{
   VixDiskLibConnection conn = (VixDiskLibConnection)(size_t)connHandle;
   const char *cPath;
   VixError result;

   cPath = GETSTRING(path);
   result = VixDiskLib_CheckRepair(conn, cPath, repair);
   FREESTRING(cPath, path);
   return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Perturb_Enable --
 *
 *     JNI implementation for Fault Injection control of
 *     PerturbEnable.  The arguments are:
 *
 *     fName - Pointer to the name of the function to be replaced.
 *     enable - zero = disable, 1 = enable.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT void JNICALL
Java_com_vmware_jvix_jDiskLibImpl_PerturbEnableJNI(JNIEnv *env,
                                                    jobject obj,
                                                    jstring fName,
                                                    jint enable)
{
   int enableIt = (int) enable;
   const char *funcName = GETSTRING(fName);

   Perturb_Enable(funcName, enableIt);
   FREESTRING(funcName, fName);
}

/*
 *-----------------------------------------------------------------------------
 *
 * VixDiskLib_SetInjectedFault --
 *
 *     JNI implementation for control of injected faults.  The args are:
 *
 *     faultID    - One of the members of enum diskLibFaultInjection.
 *     enabled    - Zero is disable, one is enable.
 *     faultError - The error value to be returned from the fault point.
 *
 *-----------------------------------------------------------------------------
 */

JNIEXPORT jlong JNICALL
Java_com_vmware_jvix_jDiskLibImpl_SetInjectedFaultJNI(JNIEnv *env,
                                                       jobject obj,
                                                       jint faultID,
                                                       jint enabled,
                                                       jint faultError)
{
   int faultIDLocal = (int) faultID;
   int enabledLocal = (int) enabled;
   int faultErrorLocal = (int) faultError;

   return VixDiskLib_SetInjectedFault(faultIDLocal, enabledLocal,
                                      faultErrorLocal);
}
