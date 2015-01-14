c = Waah::Canvas.new 800, 800

img = Waah::Image.load ARGV[0]
font = Waah::Font.load ARGV[1]

# set the color to black
c.color 0, 0, 0
# create a text path
c.text 10, 10, "Ruby rules"
# fill the path with the specified color
c.fill
  
# use a bold font
c.font "Serif", :bold
c.text 10, 20, "Ruby rules"
c.fill
  
# use an italic font
c.font "Sans Serif", :normal, :italic
c.text 10, 30, "Ruby rules"
c.fill

# rotate everything drawn in the block
c.rotate 0.5 do
  # create a rectangular path
  c.rect 20, 20, 600, 600
  # set color to red
  c.color 0xff, 0, 0
  # stroke the border of the path
  c.stroke
end
  
# move everything drawn in the block
c.translate 150, 10 do
  # create a circular path
  c.circle 150, 150, 100
  # use an image as fill
  c.image img
  # fill the circle
  c.fill
end

# use an image as background for text
c.image img
# use the specified font, loaded from a file
c.font font
# change the font size
c.font_size 50.0
c.text 5.0, 260.0, "The sky is"
c.fill

c.text 5.0, 300.0, "the limit"
c.fill

# save canvas content to an PNG file
c.snapshot.to_png "example1.png"
