qt_add_lupdate(Win_Switcher TS_FILES zh_CN.ts
    OPTIONS -locations relative)
qt_add_lrelease(Win_Switcher TS_FILES zh_CN.ts QM_FILES_OUTPUT_VARIABLE qm_files)
set_source_files_properties(${qm_files} PROPERTIES QT_RESOURCE_ALIAS "zh_CN.qm")
qt_add_resources(Win_Switcher "translations" PREFIX "/translations" FILES ${qm_files})
