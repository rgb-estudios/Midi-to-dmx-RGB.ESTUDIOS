from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaMidiShowIntegration.cpp')
text = path.read_text(encoding='utf-8')
old = '''  const auto apply_midi_panic = [&]() {\n    mPendingShowMidiEvent.reset();\n    aeyla::runtime::ShowMidiEvent ignored{};\n    while(mShowMidiIngress.try_consume(ignored))\n    {\n    }\n    mTakeScheduler.stop_reset();\n    mTakeScheduler.disarm();\n    mActiveTakeSongIndex.store(-1, std::memory_order_release);\n    mModel.release_transients();\n    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::operator_disarm);\n    mModel.set_blackout(true);\n    mParamBlackout.store(true, std::memory_order_release);\n    SetShowMidiMessage(\n        "PANIC MIDI · APAGÓN ACTIVO · salida desarmada · rearme manual");\n  };\n'''
new = '''  const auto apply_midi_panic = [&]() {\n    mPendingShowMidiEvent.reset();\n    aeyla::runtime::ShowMidiEvent ignored{};\n    while(mShowMidiIngress.try_consume(ignored))\n    {\n    }\n\n    // R10.5: N41/PANIC and the UI APAGÓN TOTAL are one physical contract.\n    // Do not create a third safety mode by disarming the scheduler here.\n    // The Art-Net worker keeps its lease/carrier and masks every source with\n    // continuous zero DMX until the operator explicitly releases APAGÓN.\n    mModel.release_transients();\n    mModel.set_blackout(true);\n    mParamBlackout.store(true, std::memory_order_release);\n    mArtNetOutput.set_blackout_latched(true);\n    SetShowMidiMessage(\n        "PANIC MIDI · APAGÓN TOTAL · DMX 0 continuo · ARM conservado");\n  };\n'''
count = text.count(old)
if count != 1:
    raise SystemExit(f'PANIC block expected once, found {count}')
path.write_text(text.replace(old, new), encoding='utf-8')
print('R10.5 PANIC semantics patched')
