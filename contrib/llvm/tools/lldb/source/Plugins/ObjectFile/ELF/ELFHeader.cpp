//===-- ELFHeader.cpp ----------------------------------------- -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <cstring>

#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/Section.h"
#include "lldb/Core/Stream.h"

#include "ELFHeader.h"

using namespace elf;
using namespace lldb;
using namespace llvm::ELF;

//------------------------------------------------------------------------------
// Static utility functions.
//
// GetMaxU64 and GetMaxS64 wrap the similarly named methods from DataExtractor
// with error handling code and provide for parsing a sequence of values.
static bool
GetMaxU64(const lldb_private::DataExtractor &data,
          lldb::offset_t *offset,
          uint64_t *value,
          uint32_t byte_size)
{
    const lldb::offset_t saved_offset = *offset;
    *value = data.GetMaxU64(offset, byte_size);
    return *offset != saved_offset;
}

static bool
GetMaxU64(const lldb_private::DataExtractor &data, 
          lldb::offset_t *offset,
          uint64_t *value,
          uint32_t byte_size,
          uint32_t count) 
{
    lldb::offset_t saved_offset = *offset;

    for (uint32_t i = 0; i < count; ++i, ++value)
    {
        if (GetMaxU64(data, offset, value, byte_size) == false) 
        {
            *offset = saved_offset;
            return false;
        }
    }
    return true;
}

static bool
GetMaxS64(const lldb_private::DataExtractor &data, 
          lldb::offset_t *offset,
          int64_t *value,
          uint32_t byte_size)
{
    const lldb::offset_t saved_offset = *offset;
    *value = data.GetMaxS64(offset, byte_size);
    return *offset != saved_offset;
}

static bool
GetMaxS64(const lldb_private::DataExtractor &data, 
          lldb::offset_t *offset,
          int64_t *value,
          uint32_t byte_size,
          uint32_t count) 
{
    lldb::offset_t saved_offset = *offset;

    for (uint32_t i = 0; i < count; ++i, ++value)
    {
        if (GetMaxS64(data, offset, value, byte_size) == false) 
        {
            *offset = saved_offset;
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// ELFHeader

ELFHeader::ELFHeader()
{
    memset(this, 0, sizeof(ELFHeader)); 
}

ByteOrder
ELFHeader::GetByteOrder() const 
{
    if (e_ident[EI_DATA] == ELFDATA2MSB)
        return eByteOrderBig;
    if (e_ident[EI_DATA] == ELFDATA2LSB)
        return eByteOrderLittle;
    return eByteOrderInvalid;
}

bool
ELFHeader::Parse(lldb_private::DataExtractor &data, lldb::offset_t *offset) 
{
    // Read e_ident.  This provides byte order and address size info.
    if (data.GetU8(offset, &e_ident, EI_NIDENT) == NULL)
        return false;

    const unsigned byte_size = Is32Bit() ? 4 : 8;
    data.SetByteOrder(GetByteOrder());
    data.SetAddressByteSize(byte_size);

    // Read e_type and e_machine.
    if (data.GetU16(offset, &e_type, 2) == NULL)
        return false;

    // Read e_version.
    if (data.GetU32(offset, &e_version, 1) == NULL)
        return false;

    // Read e_entry, e_phoff and e_shoff.
    if (GetMaxU64(data, offset, &e_entry, byte_size, 3) == false)
        return false;

    // Read e_flags.
    if (data.GetU32(offset, &e_flags, 1) == NULL)
        return false;

    // Read e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum and
    // e_shstrndx.
    if (data.GetU16(offset, &e_ehsize, 6) == NULL)
        return false;

    return true;
}

bool
ELFHeader::MagicBytesMatch(const uint8_t *magic)
{
    return memcmp(magic, ElfMagic, strlen(ElfMagic)) == 0;
}

unsigned
ELFHeader::AddressSizeInBytes(const uint8_t *magic)
{
    unsigned address_size = 0;

    switch (magic[EI_CLASS]) 
    {
    case ELFCLASS32:
        address_size = 4;
        break;
            
    case ELFCLASS64:
        address_size = 8;
        break;
    }
    return address_size;
}

unsigned
ELFHeader::GetRelocationJumpSlotType() const
{
    unsigned slot = 0;

    switch (e_machine)
    {
    default:
        assert(false && "architecture not supported");
        break;
    case EM_PPC:
        slot = R_PPC_JMP_SLOT;
        break;
    case EM_PPC64:
        slot = R_PPC64_JMP_SLOT;
        break;
    case EM_386:
    case EM_IAMCU: // FIXME: is this correct?
        slot = R_386_JUMP_SLOT;
        break;
    case EM_X86_64:
        slot = R_X86_64_JUMP_SLOT;
        break;
    case EM_ARM:
        slot = R_ARM_JUMP_SLOT;
        break;
    case EM_HEXAGON:
        slot = R_HEX_JMP_SLOT;
        break;
    case EM_AARCH64:
        slot = R_AARCH64_JUMP_SLOT;
        break;
    case EM_MIPS:
        slot = R_MIPS_JUMP_SLOT;
        break;
    }

    return slot;
}

//------------------------------------------------------------------------------
// ELFSectionHeader

ELFSectionHeader::ELFSectionHeader() 
{
    memset(this, 0, sizeof(ELFSectionHeader));
}

bool
ELFSectionHeader::Parse(const lldb_private::DataExtractor &data,
                        lldb::offset_t *offset)
{
    const unsigned byte_size = data.GetAddressByteSize();

    // Read sh_name and sh_type.
    if (data.GetU32(offset, &sh_name, 2) == NULL)
        return false;

    // Read sh_flags.
    if (GetMaxU64(data, offset, &sh_flags, byte_size) == false)
        return false;

    // Read sh_addr, sh_off and sh_size.
    if (GetMaxU64(data, offset, &sh_addr, byte_size, 3) == false)
        return false;

    // Read sh_link and sh_info.
    if (data.GetU32(offset, &sh_link, 2) == NULL)
        return false;

    // Read sh_addralign and sh_entsize.
    if (GetMaxU64(data, offset, &sh_addralign, byte_size, 2) == false)
        return false;

    return true;
}

//------------------------------------------------------------------------------
// ELFSymbol

ELFSymbol::ELFSymbol() 
{
    memset(this, 0, sizeof(ELFSymbol));
}

#define ENUM_TO_CSTR(e) case e: return #e

const char *
ELFSymbol::bindingToCString(unsigned char binding)
{
    switch (binding)
    {
    ENUM_TO_CSTR(STB_LOCAL);
    ENUM_TO_CSTR(STB_GLOBAL);
    ENUM_TO_CSTR(STB_WEAK);
    ENUM_TO_CSTR(STB_LOOS);
    ENUM_TO_CSTR(STB_HIOS);
    ENUM_TO_CSTR(STB_LOPROC);
    ENUM_TO_CSTR(STB_HIPROC);
    }
    return "";
}

const char *
ELFSymbol::typeToCString(unsigned char type)
{
    switch (type)
    {
    ENUM_TO_CSTR(STT_NOTYPE);
    ENUM_TO_CSTR(STT_OBJECT);
    ENUM_TO_CSTR(STT_FUNC);
    ENUM_TO_CSTR(STT_SECTION);
    ENUM_TO_CSTR(STT_FILE);
    ENUM_TO_CSTR(STT_COMMON);
    ENUM_TO_CSTR(STT_TLS);
    ENUM_TO_CSTR(STT_GNU_IFUNC);
    ENUM_TO_CSTR(STT_HIOS);
    ENUM_TO_CSTR(STT_LOPROC);
    ENUM_TO_CSTR(STT_HIPROC);
    }
    return "";
}

const char *
ELFSymbol::sectionIndexToCString (elf_half shndx,
                                  const lldb_private::SectionList *section_list)
{
    switch (shndx)
    {
    ENUM_TO_CSTR(SHN_UNDEF);
    ENUM_TO_CSTR(SHN_LOPROC);
    ENUM_TO_CSTR(SHN_HIPROC);
    ENUM_TO_CSTR(SHN_LOOS);
    ENUM_TO_CSTR(SHN_HIOS);
    ENUM_TO_CSTR(SHN_ABS);
    ENUM_TO_CSTR(SHN_COMMON);
    ENUM_TO_CSTR(SHN_XINDEX);
    default:
        {
            const lldb_private::Section *section = section_list->GetSectionAtIndex(shndx).get();
            if (section)
                return section->GetName().AsCString("");
        }
        break;
    }
    return "";
}

void
ELFSymbol::Dump (lldb_private::Stream *s,
                 uint32_t idx,
                 const lldb_private::DataExtractor *strtab_data,
                 const lldb_private::SectionList *section_list)
{
    s->Printf("[%3u] 0x%16.16" PRIx64 " 0x%16.16" PRIx64 " 0x%8.8x 0x%2.2x (%-10s %-13s) 0x%2.2x 0x%4.4x (%-10s) %s\n",
              idx,
              st_value,
              st_size,
              st_name,
              st_info,
              bindingToCString (getBinding()),
              typeToCString (getType()),
              st_other,
              st_shndx,
              sectionIndexToCString (st_shndx, section_list),
              strtab_data ? strtab_data->PeekCStr(st_name) : "");
}

bool
ELFSymbol::Parse(const lldb_private::DataExtractor &data, lldb::offset_t *offset)
{
    const unsigned byte_size = data.GetAddressByteSize();
    const bool parsing_32 = byte_size == 4;

    // Read st_name.
    if (data.GetU32(offset, &st_name, 1) == NULL)
        return false;

    if (parsing_32) 
    {
        // Read st_value and st_size.
        if (GetMaxU64(data, offset, &st_value, byte_size, 2) == false)
            return false;

        // Read st_info and st_other.
        if (data.GetU8(offset, &st_info, 2) == NULL)
            return false;
            
        // Read st_shndx.
        if (data.GetU16(offset, &st_shndx, 1) == NULL)
            return false;
    }
    else 
    {
        // Read st_info and st_other.
        if (data.GetU8(offset, &st_info, 2) == NULL)
            return false;
            
        // Read st_shndx.
        if (data.GetU16(offset, &st_shndx, 1) == NULL)
            return false;

        // Read st_value and st_size.
        if (data.GetU64(offset, &st_value, 2) == NULL)
            return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// ELFProgramHeader

ELFProgramHeader::ELFProgramHeader() 
{
    memset(this, 0, sizeof(ELFProgramHeader));
}

bool
ELFProgramHeader::Parse(const lldb_private::DataExtractor &data, 
                        lldb::offset_t *offset)
{
    const uint32_t byte_size = data.GetAddressByteSize();
    const bool parsing_32 = byte_size == 4;

    // Read p_type;
    if (data.GetU32(offset, &p_type, 1) == NULL)
        return false;

    if (parsing_32) {
        // Read p_offset, p_vaddr, p_paddr, p_filesz and p_memsz.
        if (GetMaxU64(data, offset, &p_offset, byte_size, 5) == false)
            return false;

        // Read p_flags.
        if (data.GetU32(offset, &p_flags, 1) == NULL)
            return false;

        // Read p_align.
        if (GetMaxU64(data, offset, &p_align, byte_size) == false)
            return false;
    }
    else {
        // Read p_flags.
        if (data.GetU32(offset, &p_flags, 1) == NULL)
            return false;

        // Read p_offset, p_vaddr, p_paddr, p_filesz, p_memsz and p_align.
        if (GetMaxU64(data, offset, &p_offset, byte_size, 6) == false)
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// ELFDynamic

ELFDynamic::ELFDynamic() 
{ 
    memset(this, 0, sizeof(ELFDynamic)); 
}

bool
ELFDynamic::Parse(const lldb_private::DataExtractor &data, lldb::offset_t *offset)
{
    const unsigned byte_size = data.GetAddressByteSize();
    return GetMaxS64(data, offset, &d_tag, byte_size, 2);
}

//------------------------------------------------------------------------------
// ELFRel

ELFRel::ELFRel()
{
    memset(this, 0, sizeof(ELFRel));
}

bool
ELFRel::Parse(const lldb_private::DataExtractor &data, lldb::offset_t *offset)
{
    const unsigned byte_size = data.GetAddressByteSize();

    // Read r_offset and r_info.
    if (GetMaxU64(data, offset, &r_offset, byte_size, 2) == false)
        return false;

    return true;
}

//------------------------------------------------------------------------------
// ELFRela

ELFRela::ELFRela()
{
    memset(this, 0, sizeof(ELFRela));
}

bool
ELFRela::Parse(const lldb_private::DataExtractor &data, lldb::offset_t *offset)
{
    const unsigned byte_size = data.GetAddressByteSize();

    // Read r_offset and r_info.
    if (GetMaxU64(data, offset, &r_offset, byte_size, 2) == false)
        return false;

    // Read r_addend;
    if (GetMaxS64(data, offset, &r_addend, byte_size) == false)
        return false;

    return true;
}


