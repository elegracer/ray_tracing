Single frame denoising example. Denoise all AOVs (-A) for a single image:
optixDenoiser -a data/albedo.exr \
              -A data/diffuse.exr -A data/glossy.exr -A data/specular.exr \
              -o result.exr data/beauty.exr

When adding (compositing) the RGB pixels in the output files
    result_diffuse_denoised.exr, result_glossy_denoised.exr and result_specular_denoised.exr,
the resulting image should look very close to result.exr (the denoised beauty layer).

---------------------------------------------------------------------------------------------------
Temporal denoising
optixDenoiser -F 1-20 -a motiondata/soane-BSDF-+++.exr \
                      -n motiondata/soane-Normal-+++.exr \
                      -f motiondata/soane-Flow-+++.exr \
                      -o result-+++.exr motiondata/soane-Beauty-+++.exr

It is recommended to add -e -8.6 to the command line when creating a clip with ffmpeg (exposure
control).
A clip can be created with ffmpeg -i result-%03d.exr -vcodec libx264 -pix_fmt yuv420p soane.mp4

To get a clip from the noisy beauty images first multiply the images by pow(2, -8.6) for example
with oiiotool (part of the OpenImageIO library):
oiiotool --frames 1-20 --cmul 2.577163e-3 motiondata/soane-Beauty-%03d.exr -o noisy%03d.exr
ffmpeg -i noisy%03d.exr -vcodec libx264 -pix_fmt yuv420p noisy.mp4

---------------------------------------------------------------------------------------------------
The sample also works with multilayer EXR files. Specify the layer name instead of the EXR filename.

---------------------------------------------------------------------------------------------------
Checking motion vectors:
Add -z to the temporal denoising command line above. With this option flow vectors from frame N
will be applied to the image in frame N-1 to get the frame N image.
With -z only the noisy beauty image and the flow vectors are required on the command line.
Flow vectors are correct when the result image of frame N is almost congruent with the noisy frame N
image (i.e. no shifts).
