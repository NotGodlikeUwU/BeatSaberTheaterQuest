"""Run the shipped GLSL through a real offscreen GPU context (requires moderngl)."""
import re
from pathlib import Path
import moderngl
import struct

source = (Path(__file__).resolve().parents[1] / "include/video_filter_glsl.hpp").read_text()
vertex, fragment = re.findall(r'R"GLSL\((.*?)\)GLSL"', source, re.S)
ctx = moderngl.create_standalone_context(require=330)
program = ctx.program(vertex_shader=vertex, fragment_shader=fragment)
vao = ctx.vertex_array(program, [])
input_texture = ctx.texture((1, 1), 4, dtype="f4")
output = ctx.texture((16, 16), 4, dtype="f4")
framebuffer = ctx.framebuffer([output])
checks = 0

def draw(rgb, correction=(1,1,1,0), extra=(1,1,1,0), oval=0, background=None):
    input_texture.write(struct.pack("4f", *rgb, 1))
    input_texture.use(0)
    program["video"].value = 0
    program["colorA"].value = correction
    program["colorB"].value = extra
    program["oval"].value = oval
    framebuffer.use()
    ctx.viewport = (0,0,16,16)
    ctx.disable(moderngl.BLEND)
    if background is not None:
        framebuffer.clear(*background, 1)
        ctx.enable(moderngl.BLEND)
        ctx.blend_func = (moderngl.ONE_MINUS_DST_COLOR, moderngl.ONE, moderngl.ZERO, moderngl.ONE)
    vao.render(mode=moderngl.TRIANGLES, vertices=3)
    values = struct.unpack("1024f", framebuffer.read(components=4, dtype="f4"))
    return lambda x,y: values[(y*16+x)*4:(y*16+x+1)*4]

def near(actual, expected, label):
    global checks
    checks += 1
    assert all(abs(a-b)<0.004 for a,b in zip(actual,expected)), (label,actual,expected)

near(draw((.2,.4,.6))(8,8),(.2,.4,.6,1),"Identity")
near(draw((.2,.4,.6),correction=(0,1,1,0))(8,8),(0,0,0,1),"Brightness")
near(draw((.2,.4,.6),correction=(1,0,1,0))(8,8),(.5,.5,.5,1),"Contrast")
luma=.2*.299+.4*.587+.6*.114
near(draw((.2,.4,.6),correction=(1,1,0,0))(8,8),(luma,luma,luma,1),"Saturation")
near(draw((1,0,0),correction=(1,1,1,1/3))(8,8),(0,1,0,1),"Hue")
near(draw((1,0,0),correction=(1,1,1,1/6))(8,8),(2/3,2/3,0,1),"PC Rodrigues hue (not HSV)")
near(draw((.2,.4,.6),correction=(1.2,1,1,0))(8,8),(.4,.6,.8,1),"PC additive brightness")
near(draw((.25,.25,.25),extra=(1,2,1,0))(8,8),(.5,.5,.5,1),"Gamma")
near(draw((.8,.4,.2),extra=(.5,1,1,0))(8,8),(.4,.2,.1,1),"Exposure")
shape=draw((1,1,1),extra=(1,1,.5,.1))
near(shape(8,8),(1,1,1,1),"Vignette center")
near(shape(0,0),(0,0,0,0),"Vignette corner")
shape=draw((1,1,1),extra=(1,1,.7,.02),oval=1)
near(shape(0,0),(0,0,0,0),"Ellipse corner")
near(shape(8,8),(1,1,1,1),"Ellipse center")
near(draw((.25,.25,.25),correction=(1.1,2,1,0),extra=(.5,2,1,0))(8,8),(.30710678,.30710678,.30710678,1),"PC exposure/gamma/contrast/brightness order")
near(draw((.2,.4,.6),background=(.3,.5,.7))(8,8),(.44,.7,.88,1),"PC soft additive blend")
print(f"{checks} shipped GLSL GPU checks passed on {ctx.info['GL_RENDERER']}")
ctx.release()
