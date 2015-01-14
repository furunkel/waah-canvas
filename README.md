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

## Examples

See ![examples](/examples)


## Documentation

Still missing.  For now, have a look at the tests or the code.

## Related Projects

![Waah App](https://github.com/furunkel/waah-app) allows you to create simple canvas applications on all
supported platforms. 



