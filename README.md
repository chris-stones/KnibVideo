KnibVideo
=========

A video format for OpenGL/GLES-2/D3D.  
Supports Alpha channel, uses very little CPU.  
Compresses using YUVA420P + LZ4 + DXT1/ETC1.  
  
A free alternative to Bink, with much lower cpu requirements,  
And a much much lower compression ratio.  
A High motion video at 1920x1080, 30fps consumes about 10Megabytes per second.
      
ETC2/DXT3 is NOT required for transparency.  
Thats all implemented in the shader on top of ETC1/DXT1.  

