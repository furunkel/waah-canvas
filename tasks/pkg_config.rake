class MRuby::Gem::Specification
  # adapted from mruby-cfunc
  def pkg_config(name, static = false, pkg_config='pkg-config')
    static = static ? '--static' : ''

    library_paths = `"#{pkg_config}" #{name} #{static} --libs-only-L`.chomp
    linker.library_paths.concat library_paths.gsub('-L', '').split

    libraries = `"#{pkg_config}" #{name} #{static} --libs-only-l`.chomp
    linker.libraries.concat libraries.gsub('-l', '').split

#     linker.flags_before_libraries ||= ''
#    linker.flags_before_libraries << `"#{pkg_config}" #{name} #{static ? '--static' : ''} --libs`.chomp

    include_paths = `"#{pkg_config}" #{name} #{static ? '--static' : ''} --cflags-only-I`
    cc.include_paths.concat include_paths.gsub('-I', '').split
  end
end
