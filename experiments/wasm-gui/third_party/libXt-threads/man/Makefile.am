libmandir = $(LIB_MAN_DIR)

libman_PRE =					\
	$(all_shadows:=.man)			\
	XtAddActions.man			\
	XtAddCallback.man			\
	XtAddEventHandler.man			\
	XtAddExposureToRegion.man		\
	XtAddGrab.man				\
	XtAddInput.man				\
	XtAllocateGC.man			\
	XtAppAddActionHook.man			\
	XtAppAddActions.man			\
	XtAppAddBlockHook.man			\
	XtAppAddConverter.man			\
	XtAppAddInput.man			\
	XtAppAddSignal.man			\
	XtAppAddTimeOut.man			\
	XtAppAddWorkProc.man			\
	XtAppCreateShell.man			\
	XtAppError.man				\
	XtAppErrorMsg.man			\
	XtAppGetErrorDatabase.man		\
	XtAppGetSelectionTimeout.man		\
	XtAppInitialize.man			\
	XtAppLock.man				\
	XtAppNextEvent.man			\
	XtAppReleaseCacheRefs.man		\
	XtAppSetExitFlag.man			\
	XtAppSetFallbackResources.man		\
	XtAppSetTypeConverter.man		\
	XtBuildEventMask.man			\
	XtCallAcceptFocus.man			\
	XtCallActionProc.man			\
	XtCallCallbacks.man			\
	XtClass.man				\
	XtConfigureWidget.man			\
	XtConvert.man				\
	XtConvertAndStore.man			\
	XtCreateApplicationContext.man		\
	XtCreateApplicationShell.man		\
	XtCreatePopupShell.man			\
	XtCreateSelectionRequest.man		\
	XtCreateWidget.man			\
	XtCreateWindow.man			\
	XtDisplay.man				\
	XtDisplayInitialize.man			\
	XtDisplayStringConversionWarning.man	\
	XtDisplayToApplicationContext.man	\
	XtError.man				\
	XtErrorMsg.man				\
	XtFindFile.man				\
	XtGetActionKeysym.man			\
	XtGetActionList.man			\
	XtGetApplicationNameAndClass.man	\
	XtGetApplicationResources.man		\
	XtGetClassExtension.man			\
	XtGetDisplays.man			\
	XtGetErrorDatabase.man			\
	XtGetGC.man				\
	XtGetKeyboardFocusWidget.man		\
	XtGetKeysymTable.man			\
	XtGetResourceList.man			\
	XtGetSelectionParameters.man		\
	XtGetSelectionRequest.man		\
	XtGetSelectionTimeout.man		\
	XtGetSelectionValue.man			\
	XtGetSelectionValueIncremental.man	\
	XtGetSubresources.man			\
	XtGrabKey.man				\
	XtHooksOfDisplay.man			\
	XtInitialize.man			\
	XtInitializeWidgetClass.man		\
	XtInsertEventTypeHandler.man		\
	XtLastEventProcessed.man		\
	XtMakeGeometryRequest.man		\
	XtMalloc.man				\
	XtManageChildren.man			\
	XtMapWidget.man				\
	XtName.man				\
	XtNameToWidget.man			\
	XtNextEvent.man				\
	XtOffset.man				\
	XtOpenApplication.man			\
	XtOwnSelection.man			\
	XtParent.man				\
	XtParseAcceleratorTable.man		\
	XtParseTranslationTable.man		\
	XtPopdown.man				\
	XtPopup.man				\
	XtProcessLock.man			\
	XtQueryGeometry.man			\
	XtRealizeWidget.man			\
	XtRegisterDrawable.man			\
	XtRegisterGrabAction.man		\
	XtReservePropertyAtom.man		\
	XtResolvePathname.man			\
	XtSessionGetToken.man			\
	XtSetArg.man				\
	XtSetKeyTranslator.man			\
	XtSetKeyboardFocus.man			\
	XtSetLanguageProc.man			\
	XtSetMultiClickTime.man			\
	XtSetSelectionParameters.man		\
	XtSetSensitive.man			\
	XtSetValues.man				\
	XtSetWMColormapWindows.man		\
	XtStringConversionWarning.man		\
	XtToolkitThreadInitialize.man		\
	XtTranslateCoords.man			\
	XtVaCreateArgsList.man

libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)

EXTRA_DIST = $(libman_PRE)
CLEANFILES = $(libman_DATA)

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

all_shadows =					\
	$(XtAddCallback_shadows)		\
	$(XtAddEventHandler_shadows)		\
	$(XtAddGrab_shadows)			\
	$(XtAppAddConverter_shadows)		\
	$(XtAppAddInput_shadows)		\
	$(XtAppAddTimeOut_shadows)		\
	$(XtAppAddWorkProc_shadows)		\
	$(XtAppCreateShell_shadows)		\
	$(XtAppError_shadows)			\
	$(XtAppErrorMsg_shadows)		\
	$(XtAppGetErrorDatabase_shadows)	\
	$(XtAppGetSelectionTimeout_shadows)	\
	$(XtAppNextEvent_shadows)		\
	$(XtNextEvent_shadows)			\
	$(XtCallCallbacks_shadows)		\
	$(XtClass_shadows)			\
	$(XtConfigureWidget_shadows)		\
	$(XtConvert_shadows)			\
	$(XtConvertAndStore_shadows)		\
	$(XtCreateApplicationContext_shadows)	\
	$(XtCreatePopupShell_shadows)		\
	$(XtCreateWidget_shadows)		\
	$(XtDisplay_shadows)			\
	$(XtDisplayInitialize_shadows)		\
	$(XtGetGC_shadows)			\
	$(XtGetResourceList_shadows)		\
	$(XtGetSelectionValue_shadows)		\
	$(XtGetSubresources_shadows)		\
	$(XtGetApplicationResources_shadows)	\
	$(XtMakeGeometryRequest_shadows)	\
	$(XtMalloc_shadows)			\
	$(XtManageChildren_shadows)		\
	$(XtMapWidget_shadows)			\
	$(XtNameToWidget_shadows)		\
	$(XtOffset_shadows)			\
	$(XtOwnSelection_shadows)		\
	$(XtParseAcceleratorTable_shadows)	\
	$(XtParseTranslationTable_shadows)	\
	$(XtPopdown_shadows)			\
	$(XtPopup_shadows)			\
	$(XtRealizeWidget_shadows)		\
	$(XtSetArg_shadows)			\
	$(XtSetKeyTranslator_shadows)		\
	$(XtSetSensitive_shadows)		\
	$(XtSetValues_shadows)			\
	$(XtGetKeysymTable_shadows)		\
	$(XtAppSetTypeConverter_shadows)	\
	$(XtAppAddActionHook_shadows)		\
	$(XtSetMultiClickTime_shadows)		\
	$(XtInsertEventTypeHandler_shadows)	\
	$(XtLastEventProcessed_shadows)		\
	$(XtAppAddSignal_shadows)		\
	$(XtAddInput_shadows)			\
	$(XtAppAddBlockHook_shadows)		\
	$(XtAppInitialize_shadows)		\
	$(XtSessionGetToken_shadows)		\
	$(XtErrorMsg_shadows)			\
	$(XtError_shadows)			\
	$(XtGetErrorDatabase_shadows)		\
	$(XtGetSelectionValueIncremental_shadows) \
	$(XtGetSelectionTimeout_shadows)	\
	$(XtCreateSelectionRequest_shadows)	\
	$(XtReservePropertyAtom_shadows)	\
	$(XtGrabKey_shadows)			\
	$(XtRegisterDrawable_shadows)		\
	$(XtAppSetExitFlag_shadows)		\
	$(XtAppLock_shadows)			\
	$(XtProcessLock_shadows)		\
	$(XtOpenApplication_shadows)

XtAddCallback_shadows =				\
	XtAddCallbacks				\
	XtRemoveCallback			\
	XtRemoveCallbacks			\
	XtRemoveAllCallbacks

XtAddEventHandler_shadows =			\
	XtAddRawEventHandler			\
	XtRemoveEventHandler			\
	XtRemoveRawEventHandler			\
	XtInsertEventHandler			\
	XtInsertRawEventHandler

XtAddGrab_shadows =				\
	XtRemoveGrab

XtAppAddConverter_shadows =			\
	XtAddConverter

XtAppAddInput_shadows =				\
	XtRemoveInput

XtAppAddTimeOut_shadows =			\
	XtRemoveTimeOut

XtAppAddWorkProc_shadows =			\
	XtRemoveWorkProc

XtAppCreateShell_shadows =			\
	XtVaAppCreateShell

XtAppError_shadows =				\
	XtAppSetErrorHandler			\
	XtAppSetWarningHandler			\
	XtAppWarning

XtAppErrorMsg_shadows =				\
	XtAppSetErrorMsgHandler			\
	XtAppSetWarningMsgHandler		\
	XtAppWarningMsg

XtAppGetErrorDatabase_shadows =			\
	XtAppGetErrorDatabaseText

XtAppGetSelectionTimeout_shadows =		\
	XtAppSetSelectionTimeout

XtAppNextEvent_shadows =			\
	XtAppPending				\
	XtAppPeekEvent				\
	XtAppProcessEvent			\
	XtDispatchEvent				\
	XtAppMainLoop

XtNextEvent_shadows =				\
	XtPending				\
	XtPeekEvent				\
	XtProcessEvent				\
	XtMainLoop

XtCallCallbacks_shadows =			\
	XtCallCallbackList			\
	XtHasCallbacks

XtClass_shadows =				\
	XtSuperclass				\
	XtIsSubclass				\
	XtCheckSubclass				\
	XtIsObject				\
	XtIsRectObj				\
	XtIsWidget				\
	XtIsComposite				\
	XtIsConstraint				\
	XtIsShell				\
	XtIsOverrideShell			\
	XtIsWMShell				\
	XtIsVendorShell				\
	XtIsTransientShell			\
	XtIsTopLevelShell			\
	XtIsApplicationShell			\
	XtIsSessionShell

XtConfigureWidget_shadows =			\
	XtMoveWidget				\
	XtResizeWidget

XtConvert_shadows =				\
	XtDirectConvert

XtConvertAndStore_shadows =			\
	XtCallConverter

XtCreateApplicationContext_shadows =		\
	XtDestroyApplicationContext		\
	XtWidgetToApplicationContext		\
	XtToolkitInitialize

XtCreatePopupShell_shadows =			\
	XtVaCreatePopupShell

XtCreateWidget_shadows =			\
	XtVaCreateWidget			\
	XtCreateManagedWidget			\
	XtVaCreateManagedWidget			\
	XtDestroyWidget

XtDisplay_shadows =				\
	XtDisplayOfObject			\
	XtScreen				\
	XtScreenOfObject			\
	XtWindow				\
	XtWindowOfObject

XtDisplayInitialize_shadows =			\
	XtOpenDisplay				\
	XtCloseDisplay				\
	XtDatabase				\
	XtScreenDatabase

XtGetGC_shadows =				\
	XtReleaseGC

XtGetResourceList_shadows =			\
	XtGetConstraintResourceList

XtGetSelectionValue_shadows =			\
	XtGetSelectionValues

XtGetSubresources_shadows =			\
	XtVaGetSubresources

XtGetApplicationResources_shadows =		\
	XtVaGetApplicationResources

XtMakeGeometryRequest_shadows =			\
	XtMakeResizeRequest

XtMalloc_shadows =				\
	XtCalloc				\
	XtRealloc				\
	XtFree					\
	XtNew					\
	XtNewString				\
	XtAsprintf

XtManageChildren_shadows =			\
	XtManageChild				\
	XtUnmanageChildren			\
	XtUnmanageChild				\
	XtIsManaged				\
	XtChangeManagedSet

XtMapWidget_shadows =				\
	XtSetMappedWhenManaged			\
	XtUnmapWidget

XtNameToWidget_shadows =			\
	XtWindowToWidget

XtOffset_shadows =				\
	XtOffsetOf				\
	XtNumber

XtOwnSelection_shadows =			\
	XtOwnSelectionIncremental		\
	XtDisownSelection

XtParseAcceleratorTable_shadows =		\
	XtInstallAccelerators			\
	XtInstallAllAccelerators

XtParseTranslationTable_shadows =		\
	XtAugmentTranslations			\
	XtOverrideTranslations			\
	XtUninstallTranslations

XtPopdown_shadows =				\
	XtCallbackPopdown			\
	MenuPopdown

XtPopup_shadows =				\
	XtPopupSpringLoaded			\
	XtCallbackNone				\
	XtCallbackNonexclusive			\
	XtCallbackExclusive			\
	MenuPopup

XtRealizeWidget_shadows =			\
	XtIsRealized				\
	XtUnrealizeWidget

XtSetArg_shadows =				\
	XtMergeArgLists

XtSetKeyTranslator_shadows =			\
	XtTranslateKeycode			\
	XtRegisterCaseConverter			\
	XtConvertCase

XtSetSensitive_shadows =			\
	XtIsSensitive

XtSetValues_shadows =				\
	XtVaSetValues				\
	XtSetSubvalues				\
	XtGetValues				\
	XtVaGetValues				\
	XtGetSubvalues				\
	XtVaGetSubvalues			\
	XtVaSetSubvalues

XtGetKeysymTable_shadows =			\
	XtKeysymToKeycodeList

XtAppSetTypeConverter_shadows =			\
	XtSetTypeConverter

XtAppAddActionHook_shadows =			\
	XtRemoveActionHook

XtSetMultiClickTime_shadows =			\
	XtGetMultiClickTime

XtInsertEventTypeHandler_shadows =		\
	XtRemoveEventTypeHandler		\
	XtRegisterExtensionSelector		\
	XtSetEventDispatcher			\
	XtDispatchEventToWidget

XtLastEventProcessed_shadows =			\
	XtLastTimestampProcessed

XtAppAddSignal_shadows =			\
	XtNoticeSignal				\
	XtRemoveSignal

XtAddInput_shadows =				\
	XtAddTimeOut				\
	XtAddWorkProc

XtAppAddBlockHook_shadows =			\
	XtRemoveBlockHook

XtAppInitialize_shadows =			\
	XtVaAppInitialize

XtSessionGetToken_shadows =			\
	XtSessionReturnToken

XtErrorMsg_shadows =				\
	XtSetErrorMsgHandler			\
	XtSetWarningMsgHandler			\
	XtWarningMsg

XtError_shadows =				\
	XtSetErrorHandler			\
	XtSetWarningHandler			\
	XtWarning

XtGetErrorDatabase_shadows =			\
	XtGetErrorDatabaseText

XtGetSelectionValueIncremental_shadows =	\
	XtGetSelectionValuesIncremental

XtGetSelectionTimeout_shadows =			\
	XtSetSelectionTimeout

XtCreateSelectionRequest_shadows =		\
	XtSendSelectionRequest			\
	XtCancelSelectionRequest

XtReservePropertyAtom_shadows =			\
	XtReleasePropertyAtom

XtGrabKey_shadows =				\
	XtUngrabKey				\
	XtGrabKeyboard				\
	XtUngrabKeyboard			\
	XtGrabButton				\
	XtUngrabButton				\
	XtGrabPointer				\
	XtUngrabPointer

XtRegisterDrawable_shadows =			\
	XtUnregisterDrawable

XtAppSetExitFlag_shadows =			\
	XtAppGetExitFlag

XtAppLock_shadows =				\
	XtAppUnlock

XtProcessLock_shadows =				\
	XtProcessUnlock

XtOpenApplication_shadows =			\
	XtVaOpenApplication

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
.man.$(LIB_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
