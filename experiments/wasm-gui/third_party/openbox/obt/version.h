#ifndef obt__version_h
#define obt__version_h

#define OBT_MAJOR_VERSION 3
#define OBT_MINOR_VERSION 6
#define OBT_MICRO_VERSION 4
#define OBT_VERSION OBT_MAJOR_VERSION.OBT_MINOR_VERSION.OBT_MICRO_VERSION

#define OBT_CHECK_VERSION(major,minor,micro) \
    (OBT_MAJOR_VERSION > (major) || \
     (OBT_MAJOR_VERSION == (major) && OBT_MINOR_VERSION > (minor)) || \
     (OBT_MAJOR_VERSION == (major) && OBT_MINOR_VERSION == (minor) && \
      OBT_MICRO_VERSION >= (micro)))

#endif
