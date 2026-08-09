SECTIONS
{
    .test_registry : {
        __test_registry_start__ = .;
        *(.test_registry)
        __test_registry_end__ = .;
    }
}
