// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "ui/Theme.h"

using namespace glint;

// ---------------------------------------------------------------------------
StepConstrainer::StepConstrainer()
{
    setFixedAspectRatio(static_cast<double>(kBaseW) / kBaseH);
    setSizeLimits(juce::roundToInt(kBaseW * 0.75f), juce::roundToInt(kBaseH * 0.75f),
                  juce::roundToInt(kBaseW * 1.5f), juce::roundToInt(kBaseH * 1.5f));
}

void StepConstrainer::checkBounds(juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous,
                                  const juce::Rectangle<int>& limits, bool top, bool left, bool bottom, bool right)
{
    ComponentBoundsConstrainer::checkBounds(bounds, previous, limits, top, left, bottom, right);
    const float wanted = static_cast<float>(bounds.getWidth()) / kBaseW;
    float best = kSteps[0];
    for (float s : kSteps)
        if (std::abs(s - wanted) < std::abs(best - wanted))
            best = s;
    bounds.setSize(juce::roundToInt(kBaseW * best), juce::roundToInt(kBaseH * best));
}

// ---------------------------------------------------------------------------
// Everything is laid out at 820 x 560 and scaled with a transform.
class GlintAudioProcessorEditor::Content final : public juce::Component {
public:
    explicit Content(GlintAudioProcessor& p)
        : proc(p)
        , cloud(p.grainEvents())
        , presetPill([&p] { return p.getProgramName(p.getCurrentProgram()); },
                     [&p](int d) {
                         const int n = p.getNumPrograms();
                         p.setCurrentProgram((p.getCurrentProgram() + d + n) % n);
                     })
        , freeze(*param(params::id::freeze))
        , density(*param(params::id::density), "Density", ui::colour::ice, lg())
        , drift(*param(params::id::drift), "Drift", ui::colour::ice, lg())
        , bloom(*param(params::id::bloom), "Bloom", ui::colour::gold, xl(), false, 100.0f)
        , space(*param(params::id::space), "Space", ui::colour::soft, lg())
        , spread(*param(params::id::spread), "Spread", ui::colour::soft, lg())
        , stereo(*param(params::id::stereoDensity), juce::String::fromUTF8("Centre \xc2\xb7 Edges"), ui::colour::soft, md(), true)
        , shimmerOn(*param(params::id::shimmerOn), "Shimmer", ui::colour::gold)
        , grainsOn(*param(params::id::grainsOn), "Grains", ui::colour::ice)
        , shimmer(*param(params::id::shimmer), params::shimmerNames(), "Shimmer", false)
        , texture(*param(params::id::texture), params::textureNames(), "Texture", false)
        , direction(*param(params::id::direction), "Direction", ui::colour::ice, sm())
        , tone(*param(params::id::tone), "Tone", ui::colour::soft, sm(), true)
        , mix(*param(params::id::mix), "Mix", ui::colour::soft, sm())
    {
        stereo.setTitle("Stereo density");
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &cloud, &presetPill, &freeze, &density, &drift, &bloom, &space, &spread, &stereo, &shimmerOn, &shimmer,
                 &grainsOn, &texture, &direction, &tone, &mix })
            addAndMakeVisible(c);
        int order = 1;
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &presetPill, &freeze, &density, &drift, &bloom, &space, &spread, &stereo, &shimmerOn, &shimmer, &grainsOn,
                 &texture, &direction, &tone, &mix })
            c->setExplicitFocusOrder(order++);
        setSize(StepConstrainer::kBaseW, StepConstrainer::kBaseH);
        refresh(); // switched-off sections start dimmed
    }

    juce::RangedAudioParameter* param(const char* id) { return proc.state().getParameter(id); }

    static ui::Knob::Style xl() { return { 112, 47, 34, 7, 8, 26, 3.5f, 15, 12, true }; }
    static ui::Knob::Style lg() { return { 72, 30, 21, 5, 4, 15, 3, 12, 12, true }; }
    static ui::Knob::Style md() { return { 52, 21, 14, 4, 3, 10, 2.5f, 11, 12, true }; }
    static ui::Knob::Style sm() { return { 40, 15, 10, 3, 2, 7.5f, 2, 11, 11, false }; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ui::colour::background);
        g.setColour(ui::colour::text);
        g.setFont(ui::font(ui::FontFace::Grotesk500, 22.0f).withExtraKerningFactor(0.32f));
        g.drawText("GLINT", 24, 24, 200, 40, juce::Justification::centredLeft, false);
        // Footer hairline.
        g.setColour(ui::colour::hairline);
        g.fillRect(24, 480, 772, 1);
    }

    void resized() override
    {
        // Header.
        freeze.setBounds(796 - 98, 27, 98, 34);
        presetPill.setBounds(freeze.getX() - 14 - 218, 27, 218, 34);
        // Cloud.
        cloud.setBounds(24, 80, 772, 180);
        // Knob row: space-between across 24+8 .. 796-8, vertically centred in 276..464.
        struct Item { juce::Component* c; int w; int h; };
        const Item row[] = { { &density, 108, 110 }, { &drift, 108, 110 }, { &bloom, 150, 150 },
                             { &space, 108, 110 }, { &spread, 108, 110 }, { &stereo, 120, 90 } };
        int total = 0;
        for (auto& it : row) total += it.w;
        const int left = 32, right = 788;
        const float gap = static_cast<float>(right - left - total) / 5.0f;
        float x = static_cast<float>(left);
        for (auto& it : row) {
            const int y = 276 + (188 - it.h) / 2;
            it.c->setBounds(juce::roundToInt(x), y, it.w, it.h);
            x += static_cast<float>(it.w) + gap;
        }
        // Footer: chips bottom-aligned at 536; each section's on/off switch
        // sits in its heading row.
        shimmer.setBounds(24, 490, shimmer.preferredWidth(), 46);
        shimmerOn.setBounds(shimmer.getX() - 1, 488, shimmerOn.preferredWidth(), 18);
        texture.setBounds(shimmer.getRight() + 20, 490, texture.preferredWidth(), 46);
        grainsOn.setBounds(texture.getX() - 1, 488, grainsOn.preferredWidth(), 18);
        mix.setBounds(796 - 104, 492, 104, 44);
        tone.setBounds(mix.getX() - 12 - 104, 492, 104, 44);
        // Direction fills the gap between the Texture chips and Tone.
        direction.setBounds(texture.getRight() + 14, 492, tone.getX() - 8 - (texture.getRight() + 14), 44);
    }

    void refresh()
    {
        auto get = [this](const char* id) { return proc.state().getRawParameterValue(id)->load(); };
        ui::ParticleCloud::State s;
        s.bloom = get(params::id::bloom) * 0.01f;
        s.drift = get(params::id::drift) * 0.01f;
        s.space = get(params::id::space) * 0.01f;
        s.frozen = get(params::id::freeze) >= 0.5f;
        cloud.setState(s);
        // Switched-off sections dim their controls (still clickable).
        const float shimmerAlpha = get(params::id::shimmerOn) >= 0.5f ? 1.0f : 0.4f;
        const float grainsAlpha = get(params::id::grainsOn) >= 0.5f ? 1.0f : 0.4f;
        if (shimmer.getAlpha() != shimmerAlpha) // the reverb + shimmer controls
            for (juce::Component* c : std::initializer_list<juce::Component*> { &shimmer, &bloom, &space })
                c->setAlpha(shimmerAlpha);
        if (texture.getAlpha() != grainsAlpha) {
            texture.setAlpha(grainsAlpha);
            direction.setAlpha(grainsAlpha);
        }
        if (lastProgram != proc.getCurrentProgram()) {
            lastProgram = proc.getCurrentProgram();
            presetPill.refresh();
        }
    }

    GlintAudioProcessor& proc;
    ui::ParticleCloud cloud;
    ui::PresetPill presetPill;
    ui::FreezeButton freeze;
    ui::Knob density, drift, bloom, space, spread, stereo;
    ui::GroupToggle shimmerOn, grainsOn;
    ui::ChipGroup shimmer, texture;
    ui::Knob direction, tone, mix;
    int lastProgram = -1;
};

// ---------------------------------------------------------------------------
GlintAudioProcessorEditor::GlintAudioProcessorEditor(GlintAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , content_(std::make_unique<Content>(p))
{
    setTitle("Glint");
    addAndMakeVisible(*content_);
    content_->addMouseListener(this, false); // right-click anywhere: size menu
    setConstrainer(&constrainer_);
    setResizable(true, true);
    setSize(StepConstrainer::kBaseW, StepConstrainer::kBaseH);
    startTimerHz(30);
}

GlintAudioProcessorEditor::~GlintAudioProcessorEditor()
{
    stopTimer();
}

void GlintAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::colour::background);
}

void GlintAudioProcessorEditor::resized()
{
    const float s = static_cast<float>(getWidth()) / StepConstrainer::kBaseW;
    content_->setTransform(juce::AffineTransform::scale(s));
    content_->setBounds(0, 0, StepConstrainer::kBaseW, StepConstrainer::kBaseH);
}

void GlintAudioProcessorEditor::setScale(float s)
{
    setSize(juce::roundToInt(StepConstrainer::kBaseW * s), juce::roundToInt(StepConstrainer::kBaseH * s));
}

void GlintAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isPopupMenu())
        return;
    juce::PopupMenu m;
    for (float s : StepConstrainer::kSteps) {
        const int pct = juce::roundToInt(s * 100.0f);
        m.addItem(juce::String(pct) + "%", true, std::abs(static_cast<float>(getWidth()) - static_cast<float>(StepConstrainer::kBaseW) * s) < 2.0f,
                  [this, s] { setScale(s); });
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
}

void GlintAudioProcessorEditor::timerCallback()
{
    content_->refresh();
}
