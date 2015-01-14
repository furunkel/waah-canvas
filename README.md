# Waah Canvas

A canvas library for MRuby. Uses [Cairo](http://cairographics.org/) as backend.

## Install

```ruby

# X11 backend
MRuby::Build.new do |conf|
  ...
  conf.gembox 'full-core'

  conf.gem github: 'furunkel/waah-canvas' do |g|
    g.configure conf, :x11, false
  end
end
```

### Cross Compiling

The project contains a Rake task to automatically download and build dependencies.

```ruby

# Example: Cross build for Android
MRuby::CrossBuild.new('androideabi') do |conf|
  toolchain :androideabi
  conf.gembox 'full-core'

  conf.gem github: 'furunkel/waah-canvas' do |g|
    g.configure conf, :android, true
  end
end

# Example: Cross build for Win32
MRuby::CrossBuild.new('win32') do |conf|
  toolchain :gcc
  conf.gembox 'full-core'

  conf.cc.command = 'i686-w64-mingw32-gcc'
  conf.archiver.command = 'i686-w64-mingw32-ar'
  conf.linker.command = 'i686-w64-mingw32-gcc'
  ENV['RANLIB'] = 'i686-w64-mingw32-ranlib'

  conf.gem github: 'furunkel/waah-canvas' do |g|
    g.configure conf, :windows, true
  end
end
```

## Example
```ruby
c = Waah::Canvas.new 800, 800

img = Waah::Image.load "some_image.jpg"
font = Waah::Font.load "some_font.ttf"

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
  c.image some_image
  # fill the circle
  c.fill
end

# use an image as background for text
c.image some_image
# use the specified font, loaded from a file
c.font some_font
# change the font size
c.font_size 50.0
c.text 5.0, 260.0, "The sky is"
c.fill

c.text 5.0, 300.0, "the limit"
c.fill

# save canvas content to an PNG file
c.snapshot.to_png "out.png"
```
### Result

## Documentation

Still missing.  For now, have a look at the tests or the code.

![Result](/test/test_simple.png?raw=true)


## Related Projects

![Waah App](https://github.com/furunkel/waah-app) allows you to create simple canvas applications on all
supported platforms. 



