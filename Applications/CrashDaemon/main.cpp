/*
 * Copyright (c) 2020, Itamar S. <itamar8910@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/Assertions.h>
#include <AK/HashMap.h>
#include <AK/LexicalPath.h>
#include <AK/LogStream.h>
#include <AK/ScopeGuard.h>
#include <LibCore/DirectoryWatcher.h>
#include <LibCoreDump/CoreDumpReader.h>
#include <LibDebug/DebugInfo.h>
#include <LibELF/Loader.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void wait_until_coredump_is_ready(const String& coredump_path)
{
    while (true) {
        struct stat statbuf;
        if (stat(coredump_path.characters(), &statbuf) < 0) {
            perror("stat");
            ASSERT_NOT_REACHED();
        }
        if (statbuf.st_mode & 0400) // Check if readable
            break;

        usleep(10000); // sleep for 10ms
    }
}

static String object_name(StringView memory_region_name)
{
    if (memory_region_name.contains("Loader.so"))
        return "Loader.so";
    if (!memory_region_name.contains(":"))
        return {};
    return memory_region_name.substring_view(0, memory_region_name.find_first_of(":").value()).to_string();
}

struct ElfObjectInfo : public RefCounted<ElfObjectInfo> {

    ElfObjectInfo(MappedFile&& file, NonnullRefPtr<ELF::Loader>&& loader, Debug::DebugInfo&& debug_info)
        : file(move(file))
        , loader(move(loader))
        , debug_info(move(debug_info))
    {
    }

    MappedFile file;
    NonnullRefPtr<ELF::Loader> loader;
    Debug::DebugInfo debug_info;
};

// FIXME: This cache has to be invalidated when libraries/programs are re-compiled.
// We can store the last-modified timestamp of the elf files in ElfObjectInfo to invalidate cache entries.
static HashMap<String, RefPtr<ElfObjectInfo>> s_debug_info_cache;

static RefPtr<ElfObjectInfo> object_info_for_region(const ELF::Core::MemoryRegionInfo* region)
{
    StringView region_name { region->region_name };

    auto name = object_name(region_name);

    String path;
    if (name.contains(".so"))
        path = String::format("/usr/lib/%s", name.characters());
    else {
        path = name;
    }

    auto cached_value = s_debug_info_cache.get(path);
    if (cached_value.has_value())
        return cached_value.value();

    struct stat st;
    if (stat(path.characters(), &st)) {
        return nullptr;
    }

    MappedFile object_file(path);
    if (!object_file.is_valid())
        return nullptr;

    auto loader = ELF::Loader::create((const u8*)object_file.data(), object_file.size());
    Debug::DebugInfo debug_info(loader);

    RefPtr<ElfObjectInfo> info = adopt(*new ElfObjectInfo(
        move(object_file),
        move(loader),
        move(debug_info)));

    s_debug_info_cache.set(path, info);
    return info;
}

static String backtrace_line(const CoreDumpReader& coredump, FlatPtr eip)
{
    auto* region = coredump.region_containing((FlatPtr)eip);
    if (!region) {
        return String::format("%p: ???", eip);
    }

    StringView region_name { region->region_name };
    if (region_name.contains("Loader.so"))
        return {};

    auto object_info = object_info_for_region(region);
    if (object_info.is_null())
        return {};

    auto func_name = object_info->loader->symbolicate(eip - region->region_start);

    auto source_position = object_info->debug_info.get_source_position(eip - region->region_start);

    auto source_position_string = String::empty();
    if (source_position.has_value())
        source_position_string = String::format(" (\033[34;1m%s\033[0m:%u)", LexicalPath(source_position.value().file_path).basename().characters(), source_position.value().line_number);

    return String::format("%p: [%s] %s%s", eip, object_name(region_name).characters(), func_name.is_null() ? "???" : func_name.characters(), source_position_string.characters());
}

static void backtrace(const String& coredump_path)
{
    size_t thread_index = 0;
    auto coredump = CoreDumpReader::create(coredump_path);
    coredump->for_each_thread_info([&thread_index, &coredump](const ELF::Core::ThreadInfo* thread_info) {
        dbgln("Backtrace for thread #{}, tid={}", thread_index++, thread_info->tid);

        uint32_t* ebp = (uint32_t*)thread_info->regs.ebp;
        uint32_t* eip = (uint32_t*)thread_info->regs.eip;
        while (ebp && eip) {

            auto line = backtrace_line(*coredump, (FlatPtr)eip);
            if (!line.is_null())
                dbgprintf("%s\n", line.characters());
            auto next_eip = coredump->peek_memory((FlatPtr)(ebp + 1));
            auto next_ebp = coredump->peek_memory((FlatPtr)(ebp));
            if (!next_ebp.has_value() || !next_eip.has_value()) {
                break;
            }

            eip = (uint32_t*)next_eip.value();
            ebp = (uint32_t*)next_ebp.value();
        }

        return IterationDecision::Continue;
    });
}

int main()
{
    static constexpr const char* coredumps_dir = "/tmp/coredump";
    mkdir(coredumps_dir, 0777);
    Core::DirectoryWatcher watcher { coredumps_dir };
    while (true) {
        auto event = watcher.wait_for_event();
        ASSERT(event.has_value());
        if (event.value().type != Core::DirectoryWatcher::Event::Type::ChildAdded)
            continue;
        auto coredump_path = event.value().child_path;
        dbgln("New coredump file: {}", coredump_path);

        wait_until_coredump_is_ready(coredump_path);

        backtrace(coredump_path);
    }
}
