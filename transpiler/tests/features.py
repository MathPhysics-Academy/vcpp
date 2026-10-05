Web VPython 3.2

# Written for vpy2cpp's tests: each line uses something the transpiler supports.
scene.caption = "A ball on a spring"
k = 4
m = 0.5
x0 = vector(1, 0, 0)
anchor = box(pos=vector(-2, 0, 0), size=vector(0.2, 1, 1), color=color.gray(0.5))
ball = sphere(pos=x0, radius=0.2, color=color.cyan, make_trail=True, retain=100)
ball.v = vector(0, 0.5, 0)
dt = 0.01
t = 0

def spring_force(x):
    return -k * (x - x0)

def kinetic(v):
    e = 0.5 * m * mag(v) ** 2
    return e

def tick():
    global t
    t += dt
    rate(100)

while t < 10:
    tick()
    F = spring_force(ball.pos)
    ball.v = ball.v + F / m * dt
    ball.pos = ball.pos + ball.v * dt
    last_x = ball.pos.x
    if ball.pos.x > 2 or not (-1 < ball.pos.y < 1):
        ball.color = color.red
    elif kinetic(ball.v) > 2:
        ball.color = color.yellow
    else:
        ball.color = color.cyan

ball.pos = vector(last_x, 0, 0)
