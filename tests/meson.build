libtexpdf_include = include_directories('..')

test_create = executable('test-create', 'test-create.c',
    dependencies: libtexpdf_dep,
    include_directories: libtexpdf_include,
)

test('Create a PDF file', test_create, args : ['pdf-test.pdf'])
