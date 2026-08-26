#include "capture/dmx_take_consolidator.h"

#include "capture/dmx_take_file_reader.h"
#include "capture/dmx_take_stream_writer.h"

#include <chrono>
#include <thread>

namespace aeyla::capture {
namespace {

constexpr std::size_t kConsolidationHighWaterFrames = 768U;
constexpr auto kConsolidationBackoff = std::chrono::milliseconds(1);

}  // namespace

DmxTakeConsolidateResult consolidate_take_range(
    const DmxTakeConsolidateRequest& request) {
  DmxTakeConsolidateResult result;
  result.target_path = request.target_path;

  if(request.source_path.empty() || request.target_path.empty()) {
    result.error = "La ruta de origen o destino del clip consolidado está vacía";
    return result;
  }
  if(request.source_path == request.target_path) {
    result.error = "La consolidación nunca puede reemplazar la TOMA ORIGINAL";
    return result;
  }
  if(request.consolidated_name.empty()) {
    result.error = "El clip consolidado necesita un nombre";
    return result;
  }

  DmxTakeFileReader reader;
  std::string error;
  if(!reader.open(request.source_path, error)) {
    result.error = "No se pudo abrir la TOMA ORIGINAL · " + error;
    return result;
  }

  const auto info = reader.info();
  if(!info.open || info.frame_count == 0U || info.frames_per_second == 0U) {
    result.error = "La TOMA ORIGINAL no contiene cuadros DMX reproducibles";
    return result;
  }
  if(request.start_frame >= request.end_frame_exclusive ||
     request.end_frame_exclusive > info.frame_count) {
    result.error = "El rango ENTRADA / SALIDA está fuera de la TOMA ORIGINAL";
    return result;
  }

  DmxTakeStreamConfig stream;
  stream.target_path = request.target_path;
  stream.song_id = info.song_id;
  stream.song_name = info.song_name;
  stream.take_name = request.consolidated_name;
  stream.source_ipv4 = info.source_ipv4;
  stream.port_address = info.port_address;
  stream.frames_per_second = info.frames_per_second;

  DmxTakeStreamWriter writer;
  if(!writer.start(stream, error)) {
    result.error = "No se pudo iniciar la consolidación · " + error;
    return result;
  }

  DmxUniverse frame{};
  for(std::uint64_t frame_index = request.start_frame;
      frame_index < request.end_frame_exclusive; ++frame_index) {
    if(!reader.read_frame(frame_index, frame, error)) {
      writer.abort();
      result.error = "Error leyendo la TOMA ORIGINAL · " + error;
      return result;
    }

    // La captura en vivo falla ante overflow; la consolidación es offline y
    // puede regular su productor. Mantenernos bajo el high-water evita usar la
    // cola como una segunda copia del clip en RAM.
    for(;;) {
      const auto status = writer.status();
      if(status.failed) {
        writer.abort();
        result.error = "Falló el escritor del clip consolidado · " + status.error;
        return result;
      }
      const auto buffered = status.frames_enqueued > status.frames_written
          ? status.frames_enqueued - status.frames_written
          : 0U;
      if(buffered < kConsolidationHighWaterFrames)
        break;
      std::this_thread::sleep_for(kConsolidationBackoff);
    }

    if(!writer.try_push_frame(frame)) {
      const auto status = writer.status();
      writer.abort();
      result.error = status.error.empty()
          ? "No se pudo encolar un cuadro durante la consolidación"
          : status.error;
      return result;
    }
  }

  if(!writer.finalize(error)) {
    result.error = "No se pudo finalizar el clip consolidado · " + error;
    return result;
  }

  // Validación de entrega: reabrir el archivo final y comprobar metadatos,
  // rango y extremos reales sin cargar el payload completo.
  DmxTakeFileReader verified;
  if(!verified.open(request.target_path, error)) {
    result.error = "El clip consolidado no superó la validación final · " + error;
    return result;
  }
  const auto verified_info = verified.info();
  const auto expected_frames = request.end_frame_exclusive - request.start_frame;
  if(verified_info.frame_count != expected_frames ||
     verified_info.frames_per_second != info.frames_per_second ||
     verified_info.port_address != info.port_address ||
     verified_info.song_id != info.song_id) {
    result.error = "El clip consolidado no coincide con los metadatos de origen";
    return result;
  }

  DmxUniverse source_first{};
  DmxUniverse source_last{};
  DmxUniverse target_first{};
  DmxUniverse target_last{};
  if(!reader.read_frame(request.start_frame, source_first, error) ||
     !reader.read_frame(request.end_frame_exclusive - 1U, source_last, error) ||
     !verified.read_frame(0U, target_first, error) ||
     !verified.read_frame(expected_frames - 1U, target_last, error) ||
     source_first != target_first || source_last != target_last) {
    result.error = "La verificación de extremos del clip consolidado falló";
    return result;
  }

  result.succeeded = true;
  result.frame_count = expected_frames;
  result.frames_per_second = verified_info.frames_per_second;
  result.duration_seconds = static_cast<double>(result.frame_count) /
                            static_cast<double>(result.frames_per_second);
  return result;
}

}  // namespace aeyla::capture
