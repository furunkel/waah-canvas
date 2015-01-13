assert('Canvas simple test') do
  c = Yeah::Canvas.new 800, 800

  bg_jpg = Yeah::Image.load "../../test/bg.jpg"
  bg_png = Yeah::Image.load "../../test/bg.png"
  ttf_font = Yeah::Font.load "../../test/Tuffy.ttf"
  otf_font = Yeah::Font.load "../../test/Tuffy.otf"

  c.color 0, 0, 0
  c.text 10, 10, "Ruby rules"
  c.fill

  c.font "Serif", :bold
  c.text 10, 20, "Ruby rules"
  c.fill

  c.font "Sans Serif", :normal, :italic
  c.text 10, 30, "Ruby rules"
  c.fill

  c.rotate 0.5 do
    c.rect 20, 20, 600, 600
    c.color 0xff, 0, 0
    c.stroke
  end

  c.translate 150, 10 do
    c.circle 150, 150, 100
    c.image bg_jpg
    c.fill
  end

  c.image bg_png
  c.font ttf_font
  c.font_size 50.0
  c.text 5.0, 260.0, "The sky is"
  c.fill

  c.font otf_font
  c.text 5.0, 300.0, "the limit"
  c.fill

  c.snapshot.to_png "../../test/test1.png"
end

assert('Canvas#pattern') do
  c = Yeah::Canvas.new 256, 256

  linear_gradient = Yeah::Pattern.linear 0.0, 0.0,  0.0, 256.0
  linear_gradient.color_stop 1, 0, 0, 0
  linear_gradient.color_stop 0, 0xff, 0xff, 0

  radial_gradient = Yeah::Pattern.radial 115.2, 102.4, 25.6, 102.4,  102.4, 128.0
  radial_gradient.color_stop 0, 0xff, 0xff, 0xff
  radial_gradient.color_stop 1, 1, 1, 1

  c.rect 0, 0, 256, 256
  c.pattern linear_gradient
  c.fill

  c.circle 128.0, 128.0, 76.8

  c.pattern radial_gradient
  c.fill

  c.snapshot.to_png "../../test/test_patterns.png"
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

assert('Font#name') do
  ttf_font = Yeah::Font.load "../../test/Tuffy.ttf"
  otf_font = Yeah::Font.load "../../test/Tuffy.otf"

  assert_equal ttf_font.name, 'Tuffy'
  assert_equal otf_font.name, 'Tuffy'
end
