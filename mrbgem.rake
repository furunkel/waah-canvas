
module ::Build
  def tmp_dir
    File.join __dir__, 'tmp', 'build'
  end
end

load File.join __dir__, 'tasks', 'build.rake'
load File.join __dir__, 'tasks', 'pkg_config.rake'

MRuby::Gem::Specification.new('mruby-waah-canvas') do |spec|
  spec.license = 'MPL 2.0'
  spec.author  = 'furunkel'

  spec.add_dependency  'mruby-string-utf8', core: 'mruby-string-utf8'

  class << self
    attr_reader :platform, :build_deps

    def add_files(files)
      self.objs.concat files.map{ |f| objfile(f.relative_path_from(dir).pathmap("#{build_dir}/%X")) }
    end

    def configure(build_conf, platform, build_deps)
      @platform = platform
      @build_deps = build_deps

      if build_deps
        ENV['PKG_CONFIG_PATH'] = "#{File.join Build.root_dir, 'lib', 'pkgconfig'}"
        linker.flags_before_libraries << "-Wl,-Bstatic"
      end

      case platform
      when :x11
        self.pkg_config 'x11'
        self.pkg_config 'xrender'
        self.pkg_config 'xext'
        self.pkg_config 'fontconfig'
      when :android
        cc.include_paths << File.join(ENV['ANDROID_NDK_HOME'], 'sources', 'android')
        linker.libraries << 'android'
        linker.libraries << 'log'
        linker.flags  << '-shared -fPIC'
      end

      self.pkg_config 'freetype2', build_deps
      self.pkg_config 'libpng', build_deps
      self.pkg_config 'pixman-1', build_deps
      self.pkg_config 'cairo', build_deps

      linker.libraries << 'jpeg'

      if build_deps
        linker.flags_before_libraries << "-Wl,-Bdynamic"
      end

      cc.defines << "YEAH_PLATFORM_#{platform.to_s.upcase}"

      if build_deps
        ENV['CC'] = build_conf.cc.command
        ENV['LD'] = build_conf.linker.command
        ENV['PKG_CONFIG_PATH'] = "#{File.join Build.root_dir, 'lib', 'pkgconfig'}"
        ENV['CFLAGS'] = build_conf.cc.flags.flatten.join(' ').gsub(/--sysroot=[^\s]+/, '')
        if platform == :android
          ENV['CFLAGS'] += " -I#{File.join ENV['ANDROID_NDK_HOME'], 'sources', 'android', 'cpufeatures'}"
          ENV['HOST'] = 'arm-linux-androideabi'
        end
        ENV['LDFLAGS'] = build_conf.linker.flags.flatten.join(' ').gsub(/--sysroot=[^\s]+/, '')
        ENV['AR'] = build_conf.archiver.command

        cc.flags.first.unshift "-I#{File.join(Build.root_dir, 'include')}"
        Rake::Task[:build_deps].invoke
      end

      build_conf.cc.include_paths.concat cc.include_paths
      build_conf.cc.defines.concat cc.defines
    end
  end

  spec.cc.flags.each{|f| f.delete '-Werror-implicit-function-declaration'}
  files = Dir.glob("#{dir}/src/*.c")

  spec.objs = add_files files

  spec.cc.include_paths << "#{dir}/include/"
  spec.export_include_paths << "#{dir}/include/"

end
