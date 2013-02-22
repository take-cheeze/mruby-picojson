MRuby::Gem::Specification.new('mruby-picojson') do |spec|
  spec.license = "BSD"
  spec.authors = 'take-cheeze'

  spec.linker.libraries << 'stdc++'
end
