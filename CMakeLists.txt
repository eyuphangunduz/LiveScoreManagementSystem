# CMakeList.txt: LiveScoreManagementSystem için CMake projesi, kaynağı ekleyin ve tanımlayın
# projeye özgü mantık burada bulunur.
#
cmake_minimum_required (VERSION 3.8)

# Destekleniyorsa MSVC derleyicileri için Çalışırken Yeniden Yükleme aracını etkinleştirin.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("LiveScoreManagementSystem")

# Kaynağı bu projenin yürütülebilir dosyasına ekleyin.
add_executable (LiveScoreManagementSystem "LiveScoreManagementSystem.cpp" "LiveScoreManagementSystem.h" "Packet.h" "Team.h" "Match.h" "LeagueManager.h" "MatchSimulator.h" "NoisyChannel.h" "Network.h" "Server.h" "Client.h" "Experiment.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET LiveScoreManagementSystem PROPERTY CXX_STANDARD 20)
endif()

# TODO: Gerekirse testleri ve yükleme hedeflerini ekleyin.
