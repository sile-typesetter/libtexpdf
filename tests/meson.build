libtexpdf_include = include_directories('..')

# FIXME:
# Above we should not add libtexpdf_cargs but if they are not added
# compiling may fail because the header inclusion is conditioned to
# defines like HAVE_CONFIG_H and others.
# In addition the header files tries to conditionally include config.h
# but this latter is private and not exported.
test_create = executable('test-create', 'test-create.c',
    c_args: libtexpdf_cargs,
    dependencies: libtexpdf_dep,
    include_directories: libtexpdf_include,
)

test('Create a PDF file', test_create, args : ['pdf-test.pdf'])
