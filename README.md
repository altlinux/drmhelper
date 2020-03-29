# libdrmhelper

A library that moves privileged operations with DRM (direct rendering manager)
to a separate binary helper. This approach allows you to split privileged code
and restrict access to it based on unix groups.

