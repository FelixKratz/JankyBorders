#pragma once
#include <CoreGraphics/CoreGraphics.h>

extern mach_port_t SLSServerPort(void* zero);
extern mach_port_t mig_get_special_reply_port(void);
extern mach_port_t mig_dealloc_special_reply_port(mach_port_t port);
extern CFTypeRef CGRegionCreateEmptyRegion(void);
extern int SLSMainConnectionID();
extern CGError SLSWindowManagementBridgeSetDelegate(void* delegate);
extern CGError SLSGetEventPort(int cid, mach_port_t* port_out);
extern CGEventRef SLEventCreateNextEvent(int cid);
extern void _CFMachPortSetOptions(CFMachPortRef mach_port, int options);

extern CGError SLSRegisterNotifyProc(void* handler, uint32_t event, void* context);
extern CGError SLSGetWindowOwner(int cid, uint32_t wid, int* out_cid);
extern CGError SLSConnectionGetPID(int cid, pid_t *pid);
extern CGError SLSRequestNotificationsForWindows(int cid, uint32_t *window_list, int window_count);

extern CGError SLSNewConnection(int zero, int *cid);
extern CGError SLSReleaseConnection(int cid);
extern CGError SLSWindowIsOrderedIn(int cid, uint32_t wid, bool* shown);
extern CGError SLSGetWindowBounds(int cid, uint32_t wid, CGRect *frame);
extern CGError CGSNewRegionWithRect(CGRect *rect, CFTypeRef *outRegion);
extern CGError SLSNewWindow(int cid, int type, float x, float y, CFTypeRef region, uint32_t *wid);
extern CGError SLSNewWindowWithOpaqueShapeAndContext(int cid, int type, CFTypeRef region, CFTypeRef opaque_shape, int options, uint64_t *tags, float x, float y, int tag_size, uint32_t *wid, void *context);
extern CGError SLSReleaseWindow(int cid, uint32_t wid);
extern CGError SLSSetWindowTags(int cid, uint32_t wid, uint64_t* tags, int tag_size);
extern CGError SLSClearWindowTags(int cid, uint32_t wid, uint64_t* tags, int tag_size);
extern CGError SLSSetWindowShape(int cid, uint32_t wid, float x_offset, float y_offset, CFTypeRef shape);
extern CGError SLSSetWindowResolution(int cid, uint32_t wid, double res);
extern CGError SLSSetWindowOpacity(int cid, uint32_t wid, bool isOpaque);
extern CGError SLSSetWindowAlpha(int cid, uint32_t wid, float alpha);
extern CGError SLSSetWindowBackgroundBlurRadius(int cid, uint32_t wid, uint32_t radius);
extern CGError SLSSetWindowShadowParameters(int cid, uint32_t wid, float std, float density, int x_offset, int y_offset);
extern CGError SLSGetWindowTransform(int cid, uint32_t wid, CGAffineTransform* transform);
extern CGError SLSSetWindowTransform(int cid, uint32_t wid, CGAffineTransform transform);

extern CGError SLSWindowSetShadowProperties(uint32_t wid, CFDictionaryRef properties);
extern CGError SLSGetWindowLevel(int cid, uint32_t wid, int64_t* level_out);
extern uint64_t SLSGetWindowSubLevel(int cid, uint32_t wid);
extern CGError SLSMoveWindowsToManagedSpace(int cid, CFArrayRef window_list, uint64_t sid);
extern CGContextRef SLWindowContextCreate(int cid, uint32_t wid, CFDictionaryRef options);

extern CFTypeRef SLSTransactionCreate(int cid);
extern CGError SLSTransactionSetWindowLevel(CFTypeRef transaction, uint32_t wid, int level);
extern CGError SLSTransactionSetWindowSubLevel(CFTypeRef transaction, uint32_t wid, int level);
extern CGError SLSTransactionSetWindowShape(CFTypeRef transaction, uint32_t wid, float x_offset, float y_offset, CFTypeRef shape);
extern CGError SLSTransactionMoveWindowWithGroup(CFTypeRef transaction, uint32_t wid, CGPoint point);
extern CGError SLSTransactionOrderWindow(CFTypeRef transaction, uint32_t wid, int order, uint32_t rel_wid);
extern CGError SLSTransactionSetWindowAlpha(CFTypeRef transaction, uint32_t wid, float alpha);
extern CGError SLSTransactionSetWindowSystemAlpha(CFTypeRef transaction, uint32_t wid, float alpha);
extern CGError SLSTransactionSetWindowTransform(CFTypeRef transaction, uint32_t wid, int not, int important, CGAffineTransform transform);
extern CGError SLSTransactionCommit(CFTypeRef transaction, int synchronous);
extern CGError SLSTransactionCommitUsingMethod(CFTypeRef transaction, uint32_t method);

extern CFArrayRef SLSCopySpacesForWindows(int cid, int selector, CFArrayRef window_list);
extern CGError SLSDisableUpdate(int cid);
extern CGError SLSReenableUpdate(int cid);

extern OSStatus _SLPSGetFrontProcess(ProcessSerialNumber *psn);
extern CGError SLSGetConnectionIDForPSN(int cid, ProcessSerialNumber *psn, int *psn_cid);
extern CGError SLSCopyConnectionProperty(int cid, int target_cid, CFStringRef key, CFTypeRef* value);

extern CFArrayRef SLSCopyWindowsWithOptionsAndTags(int cid, uint32_t owner, CFArrayRef spaces, uint32_t options, uint64_t *set_tags, uint64_t *clear_tags);

extern CFTypeRef SLSWindowQueryWindows(int cid, CFArrayRef windows, uint32_t options);
extern CFTypeRef SLSWindowQueryResultCopyWindows(CFTypeRef window_query);
extern int SLSWindowIteratorGetCount(CFTypeRef iterator);
extern bool SLSWindowIteratorAdvance(CFTypeRef iterator);
extern uint32_t SLSWindowIteratorGetParentID(CFTypeRef iterator);
extern uint32_t SLSWindowIteratorGetWindowID(CFTypeRef iterator);
extern uint64_t SLSWindowIteratorGetTags(CFTypeRef iterator);
extern uint64_t SLSWindowIteratorGetAttributes(CFTypeRef iterator);
extern int SLSWindowIteratorGetLevel(CFTypeRef iterator);

extern CFArrayRef SLSCopyManagedDisplays(int cid);
extern CFArrayRef SLSCopyManagedDisplaySpaces(int cid);
extern CFStringRef SLSCopyManagedDisplayForWindow(int cid, uint32_t wid);
extern uint64_t SLSManagedDisplayGetCurrentSpace(int cid, CFStringRef uuid);
extern CFStringRef SLSCopyActiveMenuBarDisplayIdentifier(int cid);

extern CGError SLSFlushWindowContentRegion(int cid, uint32_t wid, void* dirty);
