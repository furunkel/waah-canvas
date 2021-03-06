#!/usr/bin/env ruby
#
# mrbgems test runner
#

if __FILE__ == $0
  repository, dir = 'https://github.com/mruby/mruby.git', 'tmp/mruby'
  build_args = ARGV

  Dir.mkdir 'tmp'  unless File.exist?('tmp')
  unless File.exist?(dir)
    system "git clone #{repository} #{dir}"
  end

  exit system(%Q[cd #{dir}; MRUBY_CONFIG=#{File.expand_path __FILE__} ruby minirake -v #{build_args.join(' ')}])
end

MRuby::Build.new do |conf|
  toolchain :clang
  conf.gembox 'default'

  conf.gem File.expand_path(File.dirname(__FILE__)) do |g|
    g.configure conf, :x11, false
  end
end
