require 'tmpdir'
require 'fileutils'

include FileUtils

DEP_TARBALLS = {
  libpng:        'ftp://ftp.simplesystems.org/pub/libpng/png/src/libpng16/libpng-1.6.16.tar.xz',
  freetype:      'http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.bz2',
  zlib:          'http://zlib.net/zlib-1.2.8.tar.xz',
  pixman:        'http://cairographics.org/releases/pixman-0.32.6.tar.gz',
  expat:         'http://downloads.sourceforge.net/project/expat/expat/2.1.0/expat-2.1.0.tar.gz',
  cairo:         'http://cairographics.org/releases/cairo-1.12.18.tar.xz',
  libatomic_ops: 'https://github.com/ivmai/libatomic_ops/archive/master.tar.gz',
  fontconfig:    'http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.11.0.tar.bz2',
  libjpeg:       'http://www.ijg.org/files/jpegsrc.v9a.tar.gz',
}

module ::Build

  def root_dir
    raise 'missing platform' unless $target_platform
    File.join tmp_dir, $target_platform
  end

  def tarball(name)
    DEP_TARBALLS[name].split('/').last =~ /(\.tar.*)$/
    File.join(tmp_dir, "#{name}#{$1}")
  end

  def tarballs
    DEP_TARBALLS.map do |name, _|
      tarball(name)
    end
  end

  def dep_dir(name)
    File.join Build.tmp_dir, name.to_s
  end

  def pixman_include_cpu_features
    file_name = File.join Build.dep_dir('pixman'), 'pixman', 'pixman-arm.c'
    data = File.read file_name
    unless data.index 'cpu-features.c'
      File.write file_name, data.sub('#include <cpu-features.h>', "#include <cpu-features.h>\n#include <cpu-features.c>")
    end
  end

  def cairo_remove_localeconv
    file_name = File.join Build.dep_dir('cairo'), 'src', 'cairo-output-stream.c'
    data = File.read file_name
    if data =~ /localeconv\s*\(\)/
      new_data = data.gsub(/([a-zA-Z0-9_]+)\s*=\s*localeconv\s*\(\)\s*;/, '')
                     .gsub(/([a-zA-Z0-9_]+)\s*=\s*#{$1}->decimal_point/, %q{\1 = "."})
                     .gsub(/#include\s*<locale.h>/, '')
      File.write file_name, new_data
    end
  end


  def static_lib(name)
    if name == :pixman
      name = 'pixman-1'
    elsif name == :zlib
      name = 'z'
    elsif name.to_s =~ /lib(.*)/
      name = $1
    end
    File.join root_dir, 'lib', "lib#{name}.a"
  end

  extend self
end

directory Build.tmp_dir
directory Build.root_dir

DEP_TARBALLS.each do |name, _|

  directory Build.dep_dir(name) => [Build.tmp_dir, Build.tarball(name)] do |t|
    tarball_file = Dir["#{t.name}.tar.{bz2,xz,gz}"].first
    sh "tar xf #{tarball_file} -C #{Build.tmp_dir}"
    dirs = Dir.entries(Build.tmp_dir).map{|f| File.join(Build.tmp_dir, f)}.select{|f| File.directory? f}
    dir = dirs.grep(/#{name}/).first
    if dir.nil? && name == :libjpeg
      dir = dirs.grep(/jpeg/).first
    end
    dir.gsub!(/libatomic_ops$/, 'libatomic_ops-master')
    unless dir == t.name
      rm_r t.name if File.exists? t.name
      mv dir, t.name 
    end
  end

  file Build.tarball(name) => Build.tmp_dir do |t|
    sh "wget '#{DEP_TARBALLS[name]}' -O #{t.name}"
  end
end

task :build_deps => [Build.static_lib(:cairo), Build.static_lib(:libjpeg)]

file Build.static_lib(:zlib) => Build.dep_dir(:zlib) do
  Dir.chdir Build.dep_dir(:zlib) do
    sh "./configure --prefix=#{Build.root_dir} --static"
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:libatomic_ops) => Build.dep_dir(:libatomic_ops) do
  Dir.chdir Build.dep_dir(:libatomic_ops) do
    sh "sh autogen.sh"
    ld_flags = ENV['LDFLAGS']
    ENV['LDFLAGS'] = ''
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir}"
    ENV['LDFLAGS'] = ld_flags
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:libpng) => [Build.static_lib(:zlib), Build.dep_dir(:libpng)] do
  Dir.chdir Build.dep_dir(:libpng) do
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir}  --enable-static --enable-shared=no"
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:freetype) => [Build.dep_dir(:freetype)] do
  Dir.chdir Build.dep_dir(:freetype) do
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir} --with-zlib=yes --with-png=yes  --enable-static --disable-shared"
    sh "make -j4"
    sh "make install"
  end
end

file Build.static_lib(:pixman) => [Build.static_lib(:libpng), Build.dep_dir(:pixman)] do
  Dir.chdir Build.dep_dir(:pixman) do
    Build.pixman_include_cpu_features
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir} --enable-gtk=no --enable-static --disable-shared "
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:expat) => Build.dep_dir(:expat) do
  Dir.chdir Build.dep_dir(:expat) do
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir}"
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:fontconfig) => [Build.static_lib(:expat), Build.dep_dir(:fontconfig)] do
  Dir.chdir Build.dep_dir(:fontconfig) do
    #ENV['CFLAGS'] += '-DFONTCONFIG_PATH=\"/sdcard/.fcconfig\" -DFC_CACHEDIR=\"/sdcard/.fccache\" -DFC_DEFAULT_FONTS=\"/system/fonts\"'
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir}"
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:cairo) => [Build.static_lib(:libatomic_ops), Build.static_lib(:pixman), Build.static_lib(:freetype), Build.dep_dir(:cairo)] do
  Dir.chdir Build.dep_dir(:cairo) do
    Build.cairo_remove_localeconv
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir} --enable-gtk=no --enable-png --enable-static --disable-shared --enable-gobject=no --enable-xml=no --enable-script=no --enable-xlib=no --enable-xcb=no --enable-xlib-xcb=no --enable-xcb-shm=no --enable-qt=no --enable-fc=no --enable-ps=no --enable-pdf=no --enable-svg=no --enable-interpreter=no --enable-trace=no"
    sh "make"
    sh "make install"
  end
end

file Build.static_lib(:libjpeg) => [Build.static_lib(:zlib), Build.dep_dir(:libjpeg)] do
  Dir.chdir Build.dep_dir(:libjpeg) do
    sh "./configure --host=#{ENV['HOST']} --prefix=#{Build.root_dir}  --enable-static --enable-shared=no"
    sh "make"
    sh "make install"
  end
end
