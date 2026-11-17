"""gas2coff.py: what gcc 16 writes that clang's COFF assembler rejects."""

import gas2coff


def test_convert() -> None:
    elf = [
        '\t.section\t.rodata.str1.1,"aMS",@progbits,1',
        "\t.type\t_f, @function",
        "/APP",
        '/  57 "src/plat/pfpu.h" 1',
        "\tfistpl -8(%ebp)",
        "/NO_APP",
        "\t.local\t_op.1",
        "\t.comm\t_op.1,9,4",
        "\t.comm\t_g,4,4",
        '\t.base64\t"AAEC"',
        '\t.section\t.text.unlikely,"ax",@progbits',
    ]
    assert gas2coff.convert(elf) == [
        '\t.section .rdata,"dr"',
        "\tfistpl -8(%ebp)",
        "\t.lcomm _op.1,9,4",
        "\t.comm\t_g,4,4",
        "\t.byte 0,1,2",
        "\t.text",
    ]


if __name__ == "__main__":
    test_convert()
