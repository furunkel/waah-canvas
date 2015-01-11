assert('simple test') do
  c = Yeah::Canvas.new 800, 800

  bg = Yeah::Image.load "../../test/bg.jpg"

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
    c.image bg
    c.fill
  end

  c.font_size 50.0
  c.text 5.0, 300.0, "The sky is the limit"
  c.image bg
  c.fill

  c.snapshot.to_png "../../test/test1.png"
end
