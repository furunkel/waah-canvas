assert('Canvas simple test') do
  c = Waah::Canvas.new 800, 800

  bg_jpg = Waah::Image.load "../../test/bg.jpg"
  bg_png = Waah::Image.load "../../test/bg.png"
  ttf_font = Waah::Font.load "../../test/Tuffy.ttf"
  otf_font = Waah::Font.load "../../test/Tuffy.otf"

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
  c = Waah::Canvas.new 256, 256

  linear_gradient = Waah::Pattern.linear 0.0, 0.0,  0.0, 256.0
  linear_gradient.color_stop 1, 0, 0, 0
  linear_gradient.color_stop 0, 0xff, 0xff, 0

  radial_gradient = Waah::Pattern.radial 115.2, 102.4, 25.6, 102.4,  102.4, 128.0
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
  c = Waah::Canvas.new 123, 456
  assert_equal 123, c.width
end

assert('Canvas#height') do
  c = Waah::Canvas.new 123, 456
  assert_equal 123, c.width
end

assert('Image#width') do
  img = Waah::Image.load '../../test/bg.jpg'
  assert_equal 347, img.width
  img = Waah::Image.load '../../test/bg.png'
  assert_equal 347, img.width
end

assert('Image#height') do
  img = Waah::Image.load '../../test/bg.jpg'
  assert_equal 310, img.height
  img = Waah::Image.load '../../test/bg.png'
  assert_equal 310, img.height
end

assert('Font#name') do
  ttf_font = Waah::Font.load "../../test/Tuffy.ttf"
  otf_font = Waah::Font.load "../../test/Tuffy.otf"
  system_font = Waah::Font.list.first

  assert_equal 'Tuffy', ttf_font.name
  assert_equal 'Tuffy',otf_font.name
  assert_not_equal nil, system_font.name
end

assert('Font#family') do
  ttf_font = Waah::Font.load "../../test/Tuffy.ttf"
  otf_font = Waah::Font.load "../../test/Tuffy.otf"
  system_font = Waah::Font.list.first

  assert_nil ttf_font.family
  assert_nil otf_font.family
  assert_not_equal nil, system_font.family
end

assert('Font#style') do
  ttf_font = Waah::Font.load "../../test/Tuffy.ttf"
  otf_font = Waah::Font.load "../../test/Tuffy.otf"
  system_font = Waah::Font.list.first

  assert_nil ttf_font.style
  assert_nil otf_font.style
  assert_not_equal nil, system_font.style
end

assert('Font#list') do
  fonts = Waah::Font.list
  assert_kind_of  Array, fonts
  assert_not_equal [], fonts
end

assert('Font#find') do
  # Currently always returns a fallback font, which is ok I think
  font = Waah::Font.find("Sans Serif")
  assert_not_equal nil, font
end
