assert('simple test') do
  c = Yeah::Canvas.new 800, 800

  bg_jpg = Yeah::Image.load "../../test/bg.jpg"
  bg_png = Yeah::Image.load "../../test/bg.png"

  c.color 0, 0, 0
  c.text 10, 10, "Ruby rules"
  c.fill

  c.rotate 0.5 do
    c.rect 20, 20, 600, 600
    c.color 0xff, 0, 0
    c.stroke
  end

  c.translate 150, 10 do
    c.circle 150, 150, 200
    c.image bg_jpg
    c.fill
  end

  c.font_size 50.0
  c.text 5.0, 300.0, "The sky is the limit"
  c.image bg_png
  c.fill

  c.snapshot.to_png "../../test/test1.png"
end

assert('Canvas#width') do
  c = Yeah::Canvas.new 123, 456
  assert_equal c.width, 123
end

assert('Canvas#height') do
  c = Yeah::Canvas.new 123, 456
  assert_equal c.width, 123
end

assert('Image#width') do
  img = Yeah::Image.load '../../test/bg.jpg'
  assert_equal img.width, 347
  img = Yeah::Image.load '../../test/bg.png'
  assert_equal img.width, 347
end

assert('Image#height') do
  img = Yeah::Image.load '../../test/bg.jpg'
  assert_equal img.height, 310
  img = Yeah::Image.load '../../test/bg.png'
  assert_equal img.height, 310
end
