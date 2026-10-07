# Compiled catalogs; build.ps1 copies them to <plug-in>/locale/<lang>/LC_MESSAGES
msgfmt = find_program('msgfmt', required : true)
languages = ['pl']
foreach lang : languages
  custom_target('mo-' + lang,
    input : lang + '.po',
    output : lang + '.mo',
    command : [msgfmt, '-c', '-o', '@OUTPUT@', '@INPUT@'],
    build_by_default : true)
endforeach
