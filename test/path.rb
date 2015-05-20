assert('Canvas#path') do
  c = Waah::Canvas.new 256, 256

  c.color 0xff, 0, 0
  c.path.M(3, 3)
        .l(20, 20)
        .L(c.width / 2, c.height / 2)
        .a(5, 5, 10, 20, 1)
        .h(30)
        .v(10)
        .c(20,-30, 100, 20, 100, 40)

  c.stroke

  c.snapshot.to_png "../../test/path1.png"
end
