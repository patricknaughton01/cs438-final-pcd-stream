# Installation and Testing

We use OpenCV to read images and send individual points. Compiling using
`cmake` and running `./test <path/to/image>` should result in the image
being displayed in an OpenCV window. This will confirm that OpenCV has been
installed correctly.

# Extracting Transforms

`extract_transforms.py` will extract the transforms of the camera that was
used to collect the point clouds and output it in a form easy to read from
C++. This is to make it convenient to transform point coordinates into the
global frame so that they can be temporally aggregated.

This module reads in a list of times images were collected, and searches
the log of robot states to find the latest state that occurred before each
image collection time. It then uses forward kinematics to compute the transform
of the camera in the world frame at this time.

The output file has 12 columns of space-separated numbers. The first 9
columns are a flattened (column-major) rotation matrix, the last 3 are the
position of the camera. Row i corresponds to image i.

# Intrinsics

The intrinsics of the camera are stored in `intrinsics.txt` as space-separated
numbers in the format `fx fy cx cy`.
