 to compile shaders
 add win sdk bin to path in order to sign shaders???

 ```console 
 x@x:~/Assets$ dxc.exe -T vs_6_0 -E main .\triangle.vert.hlsl -Fo .\triangle.vert.dxil
 x@x:~/Assets$ dxc.exe -T ps_6_0 -E main .\triangle.px.hlsl -Fo .\triangle.px.dxil
 ```
