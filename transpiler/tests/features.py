Web VPython 3.2

# Written for vpy2cpp's tests: each line uses something the transpiler supports.
scene.caption = "A ball on a spring"
scene.width = scene.height = 600
scene.center = vector(0, 0, 0)
scene.forward = vector(0, -0.2, -1)
scene.range = 3
scene.userzoom = False
k = 4
m = 0.5
x0 = vector(1, 0, 0)
anchor = box(pos=vector(-2, 0, 0), size=vector(0.2, 1, 1), color=color.gray(0.5))
ball = sphere(pos=x0, radius=0.2, color=color.cyan, make_trail=True, retain=100)
ball.v = vector(0, 0.5, 0)
dt = 0.01
t = t_start = 0

def spring_force(x):
    return -k * (x - x0)

def kinetic(v, mass=m, scale=1):
    e = 0.5 * mass * mag(v) ** 2
    return scale * e

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
    elif kinetic(ball.v, scale=0.5) > 1 or kinetic(ball.v) > 2:
        ball.color = color.yellow
    else:
        ball.color = color.cyan

ball.pos = vector(last_x, 0, 0)

balls = []
for i in range(5):
    balls.append(sphere(pos=vector(i, 2, 0), radius=0.1))
heights = [0.5, 1, 1.5]
for n in range(len(heights) - 1, -1, -1):
    heights[n] = heights[n] * 2
for b in balls:
    b.color = color.green
balls[-1].radius = heights[0]

def lift(group, dy):
    for b in group:
        b.pos.y = b.pos.y + dy
    group.append(sphere(pos=vector(0, 3, 0), radius=0.1))

lift(balls, 0.5)
