build_lib(
  LIBNAME point-to-point-layout
  SOURCE_FILES
    model/point-to-point-dumbbell.cc
    model/point-to-point-grid.cc
    model/point-to-point-star.cc
    model/point-to-point-fat-tree.cc
  HEADER_FILES
    model/point-to-point-dumbbell.h
    model/point-to-point-grid.h
    model/point-to-point-star.h
    model/point-to-point-fat-tree.h
  LIBRARIES_TO_LINK
    ${libinternet}
    ${libpoint-to-point}
    ${libmobility}
)
