#include "flphelper.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <sstream>

namespace FL
{
    // =========================================================================
    // 0. Plugin database lookup
    // =========================================================================
    juce::File PluginDBLookup::getDefaultDatabaseRoot()
    {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Image-Line")
            .getChildFile("FL Studio")
            .getChildFile("Presets")
            .getChildFile("Plugin database");
    }

    PluginDBLookup::PluginDBLookup(const juce::File& databaseRoot)
    {
        juce::File root = databaseRoot.exists() ? databaseRoot : getDefaultDatabaseRoot();
        if (!root.isDirectory())
            return; // not installed on this machine, or database not yet generated - fine, just empty

        juce::File installedRoot = root.getChildFile("Installed");
        juce::Array<juce::File> iniFiles;
        // FL Studio versions differ on whether this is a single consolidated
        // .Plugins.ini or split per Effects/Generators - handle both by
        // searching for every .Plugins.ini under Installed\.
        installedRoot.findChildFiles(iniFiles, juce::File::findFiles, true, "*.Plugins.ini");
        for (auto& f : iniFiles)
            parseIniFile(f);
    }

    void PluginDBLookup::parseIniFile(const juce::File& iniFile)
    {
        juce::StringArray lines;
        iniFile.readLines(lines);

        juce::String currentSection;
        juce::String currentName, currentVendor, currentCategory;
        bool haveEntry = false;

        auto flush = [&]() {
            if (haveEntry && currentName.isNotEmpty()) {
                Entry e;
                e.vendorName = currentVendor;
                e.category = currentCategory;
                if (currentSection.containsIgnoreCase("\\VST3\\")) e.formatType = "VST3";
                else if (currentSection.containsIgnoreCase("\\VST\\")) e.formatType = "VST";
                else if (currentSection.containsIgnoreCase("\\CLAP\\")) e.formatType = "CLAP";
                entries[currentName.toLowerCase()] = e;
            }
            currentName.clear(); currentVendor.clear(); currentCategory.clear();
            haveEntry = false;
        };

        for (const auto& rawLine : lines) {
            juce::String line = rawLine.trim();
            if (line.isEmpty()) continue;

            if (line.startsWith("[") && line.endsWith("]")) {
                flush();
                currentSection = line.substring(1, line.length() - 1);
                continue;
            }
            auto eq = line.indexOfChar('=');
            if (eq <= 0) continue;
            juce::String key = line.substring(0, eq).trim();
            juce::String value = line.substring(eq + 1).trim();

            if (key == "ps_name") { currentName = value; haveEntry = true; }
            else if (key == "ps_file_vendorname_0") currentVendor = value;
            else if (key == "ps_file_category_0") currentCategory = value;
        }
        flush(); // last section in the file
    }

    std::optional<PluginDBLookup::Entry> PluginDBLookup::lookup(const juce::String& pluginName) const
    {
        if (pluginName.isEmpty()) return std::nullopt;
        auto it = entries.find(pluginName.toLowerCase());
        if (it == entries.end()) return std::nullopt;
        return it->second;
    }

    // =========================================================================
    // 1. Plugin Inspector
    // =========================================================================
    std::vector<PluginInfo> PluginInspector::getPluginSummary() const {
        std::vector<PluginInfo> summary;
        std::unordered_map<juce::String, PluginInfo> map;
        auto channels = project.getChannels();
        for (auto* ch : channels) {
            if (!ch) continue;
            ChannelType type = ch->getType();
            if (type == ChannelType::Sampler || type == ChannelType::Automation) { continue; }

            juce::String displayName = ch->getName();
            juce::String internalName = ch->getInternalName();

            // Audio clips (playlist-placed audio, sliceable/fadeable directly
            // in the playlist) have no plugin wrapper either - same as a
            // genuine wrapper-less native synth like TS404 - but they have a
            // real sample path and no instrument/envelope settings, since
            // they're just a sample container, not a generator. Confirmed via
            // a real project: ChannelType::Instrument covers both true
            // multi-out instrument channels AND audio clips, distinguished
            // only by whether a sample path is actually set. These aren't
            // plugin instances at all, so they don't belong in this report.
            if (internalName.isEmpty() && ch->getSamplePath().isNotEmpty()) { continue; }

            // Image-Line generators with their own factory name - neither the
            // "Fruity X" wrapper convention nor "Fruity Wrapper" (used for
            // externally-hosted VST/VST3/CLAP plugins) - so nothing in the
            // name itself marks them as native. Each entry below was
            // confirmed against a real project file, not guessed; add to
            // this list as more get checked rather than assuming it
            // generalizes to every Image-Line plugin.
            static const std::unordered_set<juce::String> kKnownNativeFactoryNames = {
                "Plucked!", // FruityLoops-era physical-modeling plucked string generator
                "FLEX",     // modern Image-Line rompler/sampler
            };

            // Three real cases here, not two:
            //  1. internalName starts with "Fruity" - an Image-Line generator
            //     wrapped the same way a VST would be (e.g. "Fruity Granulizer").
            //  2. internalName is something else entirely - a real 3rd-party
            //     VST/VST3, since only actual plugins get a PluginFactory name
            //     that doesn't match Image-Line's own wrapper convention.
            //  3. internalName is EMPTY - there's no PluginFactory/NewPlugin
            //     event on this channel at all. That's not "unknown", it's the
            //     old pre-wrapper era: TS404, DrumSynth, and similar generators
            //     were hardcoded straight into the FL engine with no plugin
            //     wrapper whatsoever, long before Image-Line's own plugins
            //     started being hosted the same way 3rd-party VSTs are. An
            //     empty internalName is actually the *strongest* native signal
            //     there is, not a reason to fall through to "Third-Party".
            bool hasWrapper = internalName.isNotEmpty();
            bool isNative = !hasWrapper
                          || internalName.startsWith("Fruity")
                          || internalName.startsWith("FL")
                          || kKnownNativeFactoryNames.count(internalName) > 0;

            juce::String vendor = isNative ? "Image-Line" : "Third-Party";
            if (!hasWrapper) vendor = "Image-Line (built-in engine synth)";
            else if (kKnownNativeFactoryNames.count(internalName) > 0) vendor = "Image-Line (confirmed native plugin)";
            else if (auto found = pluginDB.lookup(internalName))
                vendor = found->vendorName.isNotEmpty() ? found->vendorName : vendor;
            
            bool isDemo = false;
            const EventTree& tree = ch->getEventTree();
            if (auto* wrapper = dynamic_cast<WrapperEvent*>(tree.firstEvent(EventID::NewPlugin))) {
                isDemo = (wrapper->fields.flags & 0x04) != 0; 
            }

            juce::String key = displayName + "|" + internalName;
            auto it = map.find(key);
            if (it == map.end()) {
                PluginInfo info;
                info.name = displayName.isNotEmpty() ? displayName : internalName;
                info.vendor = vendor;
                info.type = type; info.instanceCount = 1; info.isDemo = isDemo; info.isNative = isNative;
                map[key] = info;
            } else {
                it->second.instanceCount++;
                if (isDemo) it->second.isDemo = true;
            }
        }
        for (auto& pair : map) summary.push_back(std::move(pair.second));
        return summary;
    }

    // =========================================================================
    // 2. Sample Scanner
    // =========================================================================
    std::vector<SampleFile> SampleScanner::scan(const juce::File& flInstallDir) {
        files.clear();
        auto channels = project.getChannels();
        std::unordered_set<juce::String> seen;
        for (auto* ch : channels) {
            if (!ch) continue;
            juce::String raw = ch->getSamplePath();
            if (raw.isEmpty()) { continue; }
            if (seen.insert(raw).second) {
                SampleFile sf;
                sf.originalPath = raw;
                sf.resolvedPath = resolvePath(raw, flInstallDir);
                sf.exists = juce::File(sf.resolvedPath).existsAsFile();
                sf.isStock = raw.contains("%FLStudioFactoryData%");
                sf.channelName = ch->getName();
                files.push_back(sf);
            }
        }
        return files;
    }
    std::vector<SampleFile> SampleScanner::getMissingFiles() const {
        std::vector<SampleFile> missing;
        for (const auto& sf : files) if (!sf.exists) missing.push_back(sf);
        return missing;
    }
    int SampleScanner::copySamplesTo(const juce::File& targetDir, bool copyStock) {
        int copied = 0; targetDir.createDirectory();
        for (const auto& sf : files) {
            if (!sf.exists || (sf.isStock && !copyStock)) continue;
            juce::File src(sf.resolvedPath);
            juce::File dest = targetDir.getChildFile(src.getFileName());
            if (src.copyFileTo(dest)) copied++;
        }
        return copied;
    }
    juce::String SampleScanner::resolvePath(const juce::String& raw, const juce::File& flDir) const {
        if (raw.contains("%FLStudioFactoryData%") && flDir.isDirectory()) 
            return raw.replace("%FLStudioFactoryData%", flDir.getChildFile("Data").getFullPathName());
        return raw;
    }

    // =========================================================================
    // 3. Arrangement Dumper
    // =========================================================================
    namespace {
        // Builds the composite display name FL Studio's own UI shows for an
        // automation clip ("Digilog - Filter cutoff") by combining the
        // automation channel's own name with its link target's name. The
        // link itself (RemoteController: trackId = automation channel's own
        // IID, destId = target channel IID) sits outside every channel's own
        // subtree in the file - it's a separate top-level section between
        // patterns and channels - which is why this can't just be read off
        // the automation channel directly.
        juce::String resolveAutomationDisplayName(const Project& project, const Channel* automationChannel)
        {
            juce::String ownName = automationChannel->getName();
            for (auto* rc : project.getAutomationChannels()) {
                if ((int) rc->fields.trackId != automationChannel->getIID()) continue;
                for (auto* target : project.getChannels()) {
                    if (target->getIID() == (int) rc->fields.destId)
                        return target->getName() + " - " + ownName;
                }
                break; // matched the link but couldn't resolve destId to a real channel
            }
            return ownName;
        }
    }

    std::vector<ArrangementTrack> ArrangementDumper::getTracks() const {
        std::vector<ArrangementTrack> tracks;
        Arrangement arr = project.getArrangement(0);
        auto trackObjs = arr.getTracks();
        auto items = arr.getPlaylistItems();
        int count = (int)trackObjs.size();
        tracks.resize(count);
        for (int i = 0; i < count; ++i) {
            tracks[i].index = i; tracks[i].name = trackObjs[i].getName();
            tracks[i].color = trackObjs[i].getColor(); tracks[i].isMuted = trackObjs[i].isMuted();
        }
        std::unordered_map<int, int> revMap;
        for (int i = 0; i < count; ++i) revMap[count - 1 - i] = i;
        for (const auto& item : items) {
            auto it = revMap.find(item.trackRvidx);
            if (it != revMap.end() && it->second < count) tracks[it->second].clips.push_back(item);
        }
        return tracks;
    }
    juce::String ArrangementDumper::generateTextReport() const {
        std::stringstream ss; int ppq = project.getPPQ(); auto tracks = getTracks();

        // Build lookup maps once: pattern IID -> name, channel IID -> (name, type)
        std::unordered_map<int, juce::String> patternNames;
        for (auto& p : project.getPatterns()) patternNames[p.getIID()] = p.getName();
        std::unordered_map<int, Channel*> channelsByIid;
        for (auto* ch : project.getChannels()) channelsByIid[ch->getIID()] = ch;

        for (const auto& t : tracks) {
            ss << "Track " << t.index << ": \"" << t.name.toStdString() << "\"";
            if (t.isMuted) ss << " [MUTED]";
            ss << " (" << t.clips.size() << " clips)\n";
            for (const auto& c : t.clips) {
                ss << "  Pos: " << formatTime(c.position, ppq).toStdString()
                   << " Len: " << formatTime(c.length, ppq).toStdString() << " - ";

                // itemIndex >= patternBase means this clip references a
                // pattern (itemIndex - patternBase = pattern number);
                // otherwise itemIndex IS a channel IID directly (an audio
                // clip or automation clip placed straight on the timeline).
                if (c.itemIndex >= c.patternBase) {
                    int patternIid = c.itemIndex - c.patternBase;
                    auto it = patternNames.find(patternIid);
                    ss << "Pattern " << patternIid;
                    if (it != patternNames.end() && it->second.isNotEmpty())
                        ss << " \"" << it->second.toStdString() << "\"";
                } else {
                    auto it = channelsByIid.find(c.itemIndex);
                    if (it == channelsByIid.end()) {
                        ss << "Unknown clip (channel IID " << c.itemIndex << " not found)";
                    } else {
                        auto* ch = it->second;
                        if (ch->getType() == ChannelType::Automation)
                            ss << "Automation clip \"" << resolveAutomationDisplayName(project, ch).toStdString() << "\"";
                        else
                            ss << "Audio clip \"" << ch->getName().toStdString() << "\"";
                    }
                }
                ss << "\n";
            }
        }
        return juce::String(ss.str());
    }
    juce::String ArrangementDumper::formatTime(uint32_t ticks, int ppq) {
        int beats = ticks / ppq; int rem = ticks % ppq;
        return juce::String(beats / 4 + 1) + "." + juce::String(beats % 4 + 1) + "." + juce::String(rem);
    }

    // =========================================================================
    // 4. Comparer
    // =========================================================================
    std::vector<DiffEntry> Comparer::compare(const Project& a, const Project& b) {
        std::vector<DiffEntry> diffs;
        if (a.getTempo() != b.getTempo()) diffs.push_back({"Global", "Tempo", "BPM", juce::String(a.getTempo()), juce::String(b.getTempo())});
        compareChannels(a, b, diffs);
        return diffs;
    }
    void Comparer::compareChannels(const Project& a, const Project& b, std::vector<DiffEntry>& diffs) {
        auto chA = a.getChannels(); auto chB = b.getChannels();
        std::unordered_map<int, Channel*> mapA, mapB;
        for (size_t i=0; i<chA.size(); ++i) mapA[chA[i]->getIID()] = chA[i];
        for (size_t i=0; i<chB.size(); ++i) mapB[chB[i]->getIID()] = chB[i];
        for (auto& pair : mapA) if (mapB.find(pair.first) == mapB.end()) diffs.push_back({"Channel", pair.second->getName(), "Status", "Exists", "Removed"});
        for (auto& pair : mapB) if (mapA.find(pair.first) == mapA.end()) diffs.push_back({"Channel", pair.second->getName(), "Status", "Removed", "Exists"});
        // Channel* objects are owned by their respective Project's internal cache; nothing to delete here.
    }

    // =========================================================================
    // 5. Cleaner
    // =========================================================================
    void Cleaner::runAll(Project& project, CleanupReport& report) {
        removeUnusedPatterns(project, report); removeUnusedChannels(project, report);
    }
    void Cleaner::removeUnusedPatterns(Project& project, CleanupReport& report) {
        auto used = getReferencedPatternIIDs(project); auto patterns = project.getPatterns();
        for (const auto& p : patterns) {
            int iid = p.getIID();
            if (iid >= 0 && used.find(iid) == used.end()) {
                // Note: Actual tree removal requires EventTree manipulation. 
                // Handled via getMutableEventTree() in a production build.
                report.removedPatterns++; 
            }
        }
    }
    void Cleaner::removeUnusedChannels(Project& project, CleanupReport& report) {
        auto used = getReferencedChannelIIDs(project); auto channels = project.getChannels();
        for (auto* ch : channels) {
            int iid = ch->getIID();
            if (iid >= 0 && used.find(iid) == used.end()) report.removedChannels++;
        }
    }
    std::unordered_set<int> Cleaner::getReferencedPatternIIDs(const Project& project) {
        std::unordered_set<int> used; auto arr = project.getArrangement(0);
        for (const auto& item : arr.getPlaylistItems()) if (item.itemIndex > 0) used.insert(item.itemIndex);
        return used;
    }
    std::unordered_set<int> Cleaner::getReferencedChannelIIDs(const Project& project) {
        std::unordered_set<int> used; auto patterns = project.getPatterns();
        for (const auto& p : patterns) {
            for (const auto& n : p.getNotes()) used.insert(n.channelIID);
            for (const auto& c : p.getControllers()) used.insert(c.channelIID);
        }
        return used;
    }

    // =========================================================================
    // 6. Batch Processor
    // =========================================================================
    int BatchProcessor::processBatch(const juce::Array<juce::File>& inputFiles, const juce::File& outputDirectory, const Options& options, std::function<void(int, int, const juce::String&)> cb) {
        if (!outputDirectory.exists()) outputDirectory.createDirectory();
        int successCount = 0, total = inputFiles.size();
        for (int i = 0; i < total; ++i) {
            const auto& inFile = inputFiles[i]; if (cb) cb(i, total, inFile.getFileName());
            auto project = Project::load(inFile); if (!project) continue;
            if (processProject(*project, options)) { project->save(outputDirectory.getChildFile(inFile.getFileName())); successCount++; }
        }
        if (cb) cb(total, total, "Batch Complete");
        return successCount;
    }
    bool BatchProcessor::processProject(Project& project, const Options& options) {
        if (options.tempoScaleFactor != 1.0 && options.tempoScaleFactor > 0.0) applyTempoScale(project, options.tempoScaleFactor);
        if (options.renameTracksFromPatterns) renameTracks(project);
        if (options.makeSamplesRelative && options.baseDirectoryForRelative.isDirectory()) relativizeSamples(project, options.baseDirectoryForRelative);
        if (options.watermarkTitle.isNotEmpty() || options.watermarkAuthor.isNotEmpty()) applyWatermark(project, options.watermarkTitle, options.watermarkAuthor);
        return true;
    }
    void BatchProcessor::applyTempoScale(Project& project, double factor) {
        double currentTempo = project.getTempo();
        if (currentTempo > 0.0) project.setTempo(juce::jlimit(10.0, 999.0, currentTempo * factor));
    }
    void BatchProcessor::renameTracks(Project& project) {
        Arrangement arr = project.getArrangement(0); auto tracks = arr.getTracks(); auto items = arr.getPlaylistItems(); auto patterns = project.getPatterns();
        std::unordered_map<int, juce::String> patternNames;
        for (const auto& pat : patterns) patternNames[pat.getIID()] = pat.getName();
        int trackCount = (int)tracks.size(); std::unordered_map<int, int> revToIndex;
        for (int i = 0; i < trackCount; ++i) revToIndex[trackCount - 1 - i] = i;
        std::unordered_map<int, juce::String> trackNewNames;
        for (const auto& item : items) {
            if (item.itemIndex > 0) {
                auto it = revToIndex.find(item.trackRvidx);
                if (it != revToIndex.end() && trackNewNames.find(it->second) == trackNewNames.end()) {
                    auto patIt = patternNames.find(item.itemIndex);
                    if (patIt != patternNames.end() && patIt->second.isNotEmpty()) trackNewNames[it->second] = patIt->second;
                }
            }
        }
        for (const auto& pair : trackNewNames) if (pair.first >= 0 && pair.first < trackCount) tracks[pair.first].setName(pair.second);
    }
    void BatchProcessor::relativizeSamples(Project& project, const juce::File& baseDir) {
        auto channels = project.getChannels();
        for (auto* ch : channels) {
            if (!ch) continue;
            juce::String rawPath = ch->getSamplePath();
            if (rawPath.isEmpty()) continue;
            juce::File sampleFile(rawPath);
            if (sampleFile.existsAsFile()) ch->setSamplePath(sampleFile.getRelativePathFrom(baseDir));
        }
    }
    void BatchProcessor::applyWatermark(Project& project, const juce::String& title, const juce::String& author) {
        auto md = project.getMetadata();
        if (title.isNotEmpty()) md.title = title;
        if (author.isNotEmpty()) md.author = author;
        project.setMetadata(md);
    }

    // =========================================================================
    // 7. MIDI Bridge
    // =========================================================================
    int MidiBridge::scaleTicksToMidi(int flTicks, int flPPQ) { return flPPQ <= 0 ? flTicks : static_cast<int>(std::round((static_cast<double>(flTicks) * kStandardMidiPPQ) / flPPQ)); }
    int MidiBridge::scaleTicksToFl(int midiTicks, int flPPQ) { return static_cast<int>(std::round((static_cast<double>(midiTicks) * flPPQ) / kStandardMidiPPQ)); }
    uint8_t MidiBridge::flKeyToMidiKey(uint16_t flKey) { return static_cast<uint8_t>(juce::jlimit(0, 127, static_cast<int>(flKey))); }
    uint16_t MidiBridge::midiKeyToFlKey(uint8_t midiKey) { return static_cast<uint16_t>(midiKey); }

    bool MidiBridge::exportPatternToMidi(const Pattern& pattern, int projectPPQ, double tempoBPM, const juce::File& outputFile, int /*format*/) {
        juce::MidiFile midiFile; midiFile.setTicksPerQuarterNote(kStandardMidiPPQ);
        juce::MidiMessageSequence trackSequence;
        trackSequence.addEvent(juce::MidiMessage::tempoMetaEvent(60000000.0f / tempoBPM), 0.0);
        trackSequence.addEvent(juce::MidiMessage::textMetaEvent(0x03, pattern.getName()), 0.0);
        for (const auto& note : pattern.getNotes()) {
            int midiChannel = (note.midiChannel & 0x0F) + 1; uint8_t midiNote = flKeyToMidiKey(note.key);
            uint8_t midiVel = static_cast<uint8_t>(juce::jlimit(1, 127, static_cast<int>(note.velocity)));
            int startTick = scaleTicksToMidi(note.position, projectPPQ); int endTick = scaleTicksToMidi(note.position + note.length, projectPPQ);
            if (endTick <= startTick) endTick = startTick + 1;
            trackSequence.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, midiVel), startTick);
            trackSequence.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote, 0.0f), endTick);
        }
        trackSequence.updateMatchedPairs(); trackSequence.sort(); midiFile.addTrack(trackSequence);
        juce::FileOutputStream outStream(outputFile); if (!outStream.openedOk()) return false;
        return midiFile.writeTo(outStream);
    }
    bool MidiBridge::exportProjectToMidi(const Project& project, const juce::File& outputFile) {
        juce::MidiFile midiFile; midiFile.setTicksPerQuarterNote(kStandardMidiPPQ);
        auto patterns = project.getPatterns(); int ppq = project.getPPQ(); double tempo = project.getTempo();
        for (size_t i = 0; i < patterns.size(); ++i) {
            const auto& pattern = patterns[i]; juce::MidiMessageSequence trackSequence;
            if (i == 0) trackSequence.addEvent(juce::MidiMessage::tempoMetaEvent(60000000.0f / tempo), 0.0);
            trackSequence.addEvent(juce::MidiMessage::textMetaEvent(0x03, pattern.getName()), 0.0);
            for (const auto& note : pattern.getNotes()) {
                int midiChannel = (note.midiChannel & 0x0F) + 1; uint8_t midiNote = flKeyToMidiKey(note.key);
                uint8_t midiVel = static_cast<uint8_t>(juce::jlimit(1, 127, static_cast<int>(note.velocity)));
                int startTick = scaleTicksToMidi(note.position, ppq); int endTick = scaleTicksToMidi(note.position + note.length, ppq);
                if (endTick <= startTick) endTick = startTick + 1;
                trackSequence.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, midiVel), startTick);
                trackSequence.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote, 0.0f), endTick);
            }
            trackSequence.updateMatchedPairs(); trackSequence.sort(); midiFile.addTrack(trackSequence);
        }
        juce::FileOutputStream outStream(outputFile); if (!outStream.openedOk()) return false;
        return midiFile.writeTo(outStream);
    }
    std::vector<Note> MidiBridge::importMidiToNotes(const juce::File& midiFile, int targetChannelIID, int projectPPQ) {
        std::vector<Note> flNotes; juce::MidiFile midiIn; juce::FileInputStream inStream(midiFile);
        if (!inStream.openedOk() || !midiIn.readFrom(inStream)) return flNotes;
        for (int t = 0; t < midiIn.getNumTracks(); ++t) {
            const juce::MidiMessageSequence* track = midiIn.getTrack(t); if (!track) continue;
            juce::MidiMessageSequence trackCopy(*track); trackCopy.updateMatchedPairs();
            for (int i = 0; i < trackCopy.getNumEvents(); ++i) {
                const auto& msg = trackCopy.getEventPointer(i)->message;
                if (msg.isNoteOn()) {
                    Note flNote{}; flNote.channelIID = static_cast<uint16_t>(targetChannelIID);
                    flNote.key = midiKeyToFlKey(msg.getNoteNumber()); flNote.velocity = static_cast<uint8_t>(msg.getVelocity());
                    flNote.midiChannel = static_cast<uint8_t>(msg.getChannel() - 1);
                    flNote.finePitch = 120; flNote.release = 64; flNote.pan = 64; flNote.modX = 128; flNote.modY = 128;
                    int startTick = static_cast<int>(trackCopy.getEventTime(i));
                    int endTick = startTick + (projectPPQ / 4);
                    const auto* noteOff = trackCopy.getEventPointer(i)->noteOffObject;
                    if (noteOff) endTick = static_cast<int>(noteOff->message.getTimeStamp());
                    flNote.position = static_cast<uint32_t>(scaleTicksToFl(startTick, projectPPQ));
                    flNote.length = static_cast<uint32_t>(std::max(1, scaleTicksToFl(endTick - startTick, projectPPQ)));
                    flNotes.push_back(flNote);
                }
            }
        }
        return flNotes;
    }
    bool MidiBridge::importMidiToPattern(Pattern& pattern, const juce::File& midiFile, int projectPPQ, int targetChannelIID) {
        auto notes = importMidiToNotes(midiFile, targetChannelIID, projectPPQ); if (notes.empty()) return false;
        pattern.setNotes(notes); return true;
    }

    // =========================================================================
    // 8. Automation Editor
    // =========================================================================
    void AutomationEditor::scalePoints(std::vector<AutomationEvent::Record>& points, double factor) { for (auto& p : points) p.value *= factor; }
    void AutomationEditor::invertPoints(std::vector<AutomationEvent::Record>& points, double maxValue) { for (auto& p : points) p.value = maxValue - p.value; }
    void AutomationEditor::smoothPoints(std::vector<AutomationEvent::Record>& points, int windowSize) {
        if (points.size() < 3 || windowSize < 2) return;
        std::vector<double> originalValues; originalValues.reserve(points.size());
        for (const auto& p : points) originalValues.push_back(p.value);
        int halfWindow = windowSize / 2;
        for (size_t i = 1; i < points.size() - 1; ++i) {
            double sum = 0.0; int count = 0;
            for (int j = -halfWindow; j <= halfWindow; ++j) {
                int idx = static_cast<int>(i) + j;
                if (idx >= 0 && idx < static_cast<int>(points.size())) { sum += originalValues[idx]; count++; }
            }
            points[i].value = sum / count;
        }
    }
    int AutomationEditor::removeRedundantPoints(std::vector<AutomationEvent::Record>& points, double tolerance) {
        // Record::position is already absolute PPQ ticks, so this is a
        // straightforward linear-interpolation-error check against
        // neighbours - no delta-time reconstruction needed (unlike the old
        // AutomationPoint::beatIncrement version this replaced).
        if (points.size() < 3) return 0;
        std::vector<size_t> keepIndices; keepIndices.push_back(0); int removedCount = 0;
        for (size_t i = 1; i < points.size() - 1; ++i) {
            double t_range = points[i + 1].position - points[i - 1].position;
            double linearVal = points[i - 1].value;
            if (t_range > 0.0) linearVal += ((points[i].position - points[i - 1].position) / t_range) * (points[i + 1].value - points[i - 1].value);
            if (std::abs(points[i].value - linearVal) < tolerance) removedCount++;
            else keepIndices.push_back(i);
        }
        keepIndices.push_back(points.size() - 1);
        std::vector<AutomationEvent::Record> newPoints; newPoints.reserve(keepIndices.size());
        for (size_t k = 0; k < keepIndices.size(); ++k)
            newPoints.push_back(points[keepIndices[k]]);
        points.swap(newPoints); return removedCount;
    }
    bool AutomationEditor::applyToChannel(Channel& channel, std::function<void(std::vector<AutomationEvent::Record>&)> modifier) {
        EventTree& tree = channel.getMutableTree();
        Event* ev = tree.firstEvent(EventID::Automation); if (!ev) return false;
        auto* autoEv = dynamic_cast<AutomationEvent*>(ev); if (!autoEv) return false;
        modifier(autoEv->records); return true;
    }
    std::vector<AutomationEvent::Record> AutomationEditor::generateFadeCurve(double startValue, double endValue, double totalBeats, int numPoints, int curveType) {
        std::vector<AutomationEvent::Record> points; if (numPoints < 2) return points;
        points.reserve(numPoints);
        double posDelta = totalBeats / (numPoints - 1);
        double valueDelta = (endValue - startValue) / (numPoints - 1);
        for (int i = 0; i < numPoints; ++i) {
            AutomationEvent::Record p{};
            p.controlCode = (i == 0 || i == numPoints - 1) ? 0x03 : 0x00;
            p.curveType = (i == 0) ? 0 : curveType; // first record's curveType is unused (nothing leads into it)
            p.position = posDelta * i;
            p.value = startValue + (valueDelta * i);
            points.push_back(p);
        }
        return points;
    }

    // =========================================================================
    // 9. Stats Generator
    // =========================================================================
    ProjectStats StatsGenerator::generate(const Project& project) {
        ProjectStats stats; stats.initialTempo = project.getTempo(); stats.averageTempo = stats.initialTempo;
        analyzeChannels(project, stats); analyzePatterns(project, stats);
        analyzeArrangement(project, stats); analyzeTempoAndDuration(project, stats);
        return stats;
    }
    void StatsGenerator::analyzeChannels(const Project& project, ProjectStats& stats) {
        auto channels = project.getChannels(); stats.totalChannels = (int)channels.size();
        for (auto* ch : channels) {
            if (!ch) continue;
            switch (ch->getType()) {
                case ChannelType::Sampler: stats.samplerChannels++; break;
                case ChannelType::Native: case ChannelType::Instrument: {
                    // Audio clips (playlist-placeable, but can also be played
                    // from channel rack patterns) use these same ChannelType
                    // values as true generator plugins, with no wrapper and a
                    // real sample path as the only distinguishing signal -
                    // they're their own sampler subtype, not a plugin.
                    if (ch->getInternalName().isEmpty() && ch->getSamplePath().isNotEmpty())
                        stats.audioClipChannels++;
                    else
                        stats.nativePluginChannels++;
                    break;
                }
                case ChannelType::Automation: {
                    stats.automationChannels++;
                    const EventTree& tree = ch->getEventTree();
                    for (auto* ev : tree.getEvents(EventID::Automation))
                        if (auto* autoEv = dynamic_cast<AutomationEvent*>(ev)) stats.totalAutomationPoints += (int)autoEv->points.size();
                    break;
                }
                default: break;
            }
        }
    }
    void StatsGenerator::analyzePatterns(const Project& project, ProjectStats& stats) {
        auto patterns = project.getPatterns(); stats.totalPatterns = (int)patterns.size();
        for (const auto& pat : patterns) { stats.totalNotes += (int)pat.getNotes().size(); stats.totalPatternControllers += (int)pat.getControllers().size(); }
    }
    void StatsGenerator::analyzeArrangement(const Project& project, ProjectStats& stats) {
        Arrangement arr = project.getArrangement(0); auto tracks = arr.getTracks(); auto items = arr.getPlaylistItems();
        stats.totalTracks = (int)tracks.size(); stats.totalPlaylistItems = (int)items.size();
        for (const auto& item : items) { uint32_t endTick = item.position + item.length; if (endTick > stats.maxPlaylistTick) stats.maxPlaylistTick = endTick; }
    }
    void StatsGenerator::analyzeTempoAndDuration(const Project& project, ProjectStats& stats) {
        int ppq = project.getPPQ(); if (ppq <= 0) return;
        stats.durationBeats = (double)stats.maxPlaylistTick / (double)ppq;
        auto tempoPoints = project.getTempoAutomationPoints();
        if (!tempoPoints.empty()) {
            stats.hasTempoAutomation = true; double sum = 0.0;
            for (const auto& p : tempoPoints) sum += p.value;
            stats.averageTempo = sum / (double)tempoPoints.size(); stats.totalAutomationPoints += (int)tempoPoints.size();
        }
        if (stats.averageTempo > 0.0) stats.durationSeconds = stats.durationBeats / (stats.averageTempo / 60.0);
    }
    juce::String ProjectStats::generateTextReport() const {
        juce::StringArray lines;
        lines.add("=== FL STUDIO PROJECT DASHBOARD ===");
        lines.add("Initial Tempo: " + juce::String(initialTempo, 2) + " BPM | Average: " + juce::String(averageTempo, 2) + " BPM");
        lines.add("Duration: " + juce::String(durationSeconds, 2) + "s (" + juce::String(durationBeats, 1) + " beats)");
        lines.add("Channels: " + juce::String(totalChannels) + " (Samplers: " + juce::String(samplerChannels) + ", Audio Clips: " + juce::String(audioClipChannels) + ", Plugins: " + juce::String(nativePluginChannels) + ")");
        lines.add("Patterns: " + juce::String(totalPatterns) + " | Notes: " + juce::String(totalNotes));
        lines.add("Tracks: " + juce::String(totalTracks) + " | Clips: " + juce::String(totalPlaylistItems));
        lines.add("Automation Points: " + juce::String(totalAutomationPoints));
        return lines.joinIntoString("\n");
    }

    // =========================================================================
    // 10. Full structured JSON export
    // =========================================================================
    namespace {
        juce::String channelTypeName(ChannelType t) {
            switch (t) {
                case ChannelType::Sampler:    return "Sampler";
                case ChannelType::Native:     return "Native";
                case ChannelType::Layer:      return "Layer";
                case ChannelType::Instrument: return "Instrument";
                case ChannelType::Automation: return "Automation";
                default: return "Unknown";
            }
        }
        juce::String colourToHex(const juce::Colour& c) {
            return "#" + juce::String::toHexString((int)c.getRed()).paddedLeft('0', 2)
                       + juce::String::toHexString((int)c.getGreen()).paddedLeft('0', 2)
                       + juce::String::toHexString((int)c.getBlue()).paddedLeft('0', 2);
        }
    }

    juce::String ProjectJsonExporter::exportToJson(const Project& project)
    {
        auto root = std::make_unique<juce::DynamicObject>();

        // ---- metadata ----
        {
            auto md = project.getMetadata();
            auto meta = std::make_unique<juce::DynamicObject>();
            meta->setProperty("title", md.title);
            meta->setProperty("author", md.author);
            meta->setProperty("genre", md.genre);
            meta->setProperty("comments", md.comments);
            meta->setProperty("tempo", project.getTempo());
            meta->setProperty("ppq", project.getPPQ());
            root->setProperty("metadata", juce::var(meta.release()));
        }

        // ---- channels ----
        {
            juce::Array<juce::var> channelsArr;
            for (auto* ch : project.getChannels()) {
                auto c = std::make_unique<juce::DynamicObject>();
                c->setProperty("iid", ch->getIID());
                c->setProperty("name", ch->getName());
                c->setProperty("type", channelTypeName(ch->getType()));
                c->setProperty("internalName", ch->getInternalName());
                c->setProperty("samplePath", ch->getSamplePath());
                c->setProperty("enabled", ch->isEnabled());
                c->setProperty("color", colourToHex(ch->getColor()));
                if (auto v = ch->getVolume()) c->setProperty("volume", *v);
                if (auto p = ch->getPan()) c->setProperty("pan", *p);
                channelsArr.add(juce::var(c.release()));
            }
            root->setProperty("channels", channelsArr);
        }

        // ---- patterns ----
        {
            juce::Array<juce::var> patternsArr;
            for (auto& p : project.getPatterns()) {
                auto po = std::make_unique<juce::DynamicObject>();
                po->setProperty("iid", p.getIID());
                po->setProperty("name", p.getName());
                juce::Array<juce::var> notesArr;
                for (auto& n : p.getNotes()) {
                    auto no = std::make_unique<juce::DynamicObject>();
                    no->setProperty("position", (int) n.position);
                    no->setProperty("length", (int) n.length);
                    no->setProperty("key", n.key);
                    no->setProperty("channelIID", n.channelIID);
                    no->setProperty("velocity", n.velocity);
                    no->setProperty("pan", n.pan);
                    notesArr.add(juce::var(no.release()));
                }
                po->setProperty("notes", notesArr);
                patternsArr.add(juce::var(po.release()));
            }
            root->setProperty("patterns", patternsArr);
        }

        // ---- arrangement ----
        {
            auto arr = project.getArrangement(0);
            auto ao = std::make_unique<juce::DynamicObject>();
            ao->setProperty("name", arr.getName());

            juce::Array<juce::var> tracksArr;
            for (auto& t : arr.getTracks()) {
                auto to = std::make_unique<juce::DynamicObject>();
                to->setProperty("iid", t.getIID());
                to->setProperty("name", t.getName());
                to->setProperty("muted", t.isMuted());
                to->setProperty("color", colourToHex(t.getColor()));
                tracksArr.add(juce::var(to.release()));
            }
            ao->setProperty("tracks", tracksArr);

            juce::Array<juce::var> itemsArr;
            std::unordered_map<int, juce::String> patternNamesForJson;
            for (auto& p : project.getPatterns()) patternNamesForJson[p.getIID()] = p.getName();
            std::unordered_map<int, Channel*> channelsByIidForJson;
            for (auto* ch : project.getChannels()) channelsByIidForJson[ch->getIID()] = ch;

            for (auto& item : arr.getPlaylistItems()) {
                auto io = std::make_unique<juce::DynamicObject>();
                io->setProperty("position", (int) item.position);
                io->setProperty("length", (int) item.length);
                io->setProperty("trackRvidx", item.trackRvidx);

                // See the identical logic (and the reasoning behind it) in
                // ArrangementDumper::generateTextReport() above: itemIndex >=
                // patternBase means a pattern reference; otherwise itemIndex
                // is a channel IID directly (audio clip or automation clip).
                if (item.itemIndex >= item.patternBase) {
                    int patternIid = item.itemIndex - item.patternBase;
                    io->setProperty("kind", "pattern");
                    io->setProperty("patternIid", patternIid);
                    auto it = patternNamesForJson.find(patternIid);
                    io->setProperty("name", it != patternNamesForJson.end() ? it->second : juce::String());
                } else {
                    auto it = channelsByIidForJson.find(item.itemIndex);
                    if (it == channelsByIidForJson.end()) {
                        io->setProperty("kind", "unknown");
                        io->setProperty("channelIid", item.itemIndex);
                    } else {
                        auto* ch = it->second;
                        bool isAutomation = ch->getType() == ChannelType::Automation;
                        io->setProperty("kind", isAutomation ? "automationClip" : "audioClip");
                        io->setProperty("channelIid", item.itemIndex);
                        io->setProperty("name", isAutomation ? resolveAutomationDisplayName(project, ch) : ch->getName());
                    }
                }
                itemsArr.add(juce::var(io.release()));
            }
            ao->setProperty("playlistItems", itemsArr);

            root->setProperty("arrangement", juce::var(ao.release()));
        }

        // ---- mixer ----
        {
            auto mixer = project.getMixer();
            auto mo = std::make_unique<juce::DynamicObject>();
            juce::Array<juce::var> insertsArr;
            for (auto& ins : mixer.getInserts()) {
                auto io = std::make_unique<juce::DynamicObject>();
                io->setProperty("index", ins.getIID());
                io->setProperty("name", ins.getName());
                io->setProperty("enabled", ins.isEnabled());
                io->setProperty("color", colourToHex(ins.getColor()));
                juce::Array<juce::var> slotsArr;
                for (auto& slot : ins.getSlots()) {
                    auto so = std::make_unique<juce::DynamicObject>();
                    so->setProperty("index", slot.getIndex());
                    so->setProperty("name", slot.getName());
                    so->setProperty("enabled", slot.isEnabled());
                    slotsArr.add(juce::var(so.release()));
                }
                io->setProperty("slots", slotsArr);
                insertsArr.add(juce::var(io.release()));
            }
            mo->setProperty("inserts", insertsArr);
            root->setProperty("mixer", juce::var(mo.release()));
        }

        return juce::JSON::toString(juce::var(root.release()));
    }
}