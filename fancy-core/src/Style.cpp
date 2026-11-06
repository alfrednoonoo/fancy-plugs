#include <fancy/ui/Style.h>

#include "FancyFontData.h"

#include <numbers>

namespace fancy::ui
{

Typefaces::Typefaces()
{
    using namespace FancyFontData;
    regular = juce::Typeface::createSystemTypefaceFor (InterRegular_ttf, (size_t) InterRegular_ttfSize);
    medium = juce::Typeface::createSystemTypefaceFor (InterMedium_ttf, (size_t) InterMedium_ttfSize);
    semiBold = juce::Typeface::createSystemTypefaceFor (InterSemiBold_ttf, (size_t) InterSemiBold_ttfSize);
}

juce::Font font (Weight weight, float height)
{
    const juce::SharedResourcePointer<Typefaces> typefaces;

    const auto typeface = weight == Weight::regular  ? typefaces->regular
                        : weight == Weight::medium   ? typefaces->medium
                                                     : typefaces->semiBold;

    return juce::Font (juce::FontOptions {}.withTypeface (typeface).withHeight (height));
}

FancyLookAndFeel::FancyLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::bgBody);
    setColour (juce::PopupMenu::textColourId, colours::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::uiTeal);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, colours::bgWell);
    setColour (juce::TextEditor::textColourId, colours::textPrimary);
    setColour (juce::TextEditor::highlightColourId, colours::uiTeal);
    setColour (juce::TextEditor::outlineColourId, colours::grey5);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::uiTeal);
    setColour (juce::CaretComponent::caretColourId, colours::textPrimary);
    setColour (juce::AlertWindow::backgroundColourId, colours::bgBody);
    setColour (juce::AlertWindow::textColourId, colours::textPrimary);
    setColour (juce::AlertWindow::outlineColourId, colours::grey5);
    setColour (juce::TextButton::buttonColourId, colours::bgBody);
    setColour (juce::TextButton::textColourOffId, colours::textPrimary);
    setColour (juce::ResizableWindow::backgroundColourId, colours::bgBody);
}

juce::Typeface::Ptr FancyLookAndFeel::getTypefaceForFont (const juce::Font& requested)
{
    if (requested.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typefaces->regular;

    return LookAndFeel_V4::getTypefaceForFont (requested);
}

juce::Font FancyLookAndFeel::getPopupMenuFont()
{
    return font (Weight::regular, 14.0f);
}

namespace icons
{

namespace
{
// Glyph outlines exported from the Figma file, see design/icons.
juce::Path fitted (const char* svgPath, float viewBoxWidth, float viewBoxHeight, juce::Rectangle<float> area)
{
    auto path = juce::Drawable::parseSVGPath (svgPath);
    path.applyTransform (juce::AffineTransform::scale (area.getWidth() / viewBoxWidth, area.getHeight() / viewBoxHeight)
                             .translated (area.getX(), area.getY()));
    return path;
}
} // namespace

juce::Path power (juce::Rectangle<float> area)
{
    return fitted ("M10.5153 3.66447C11.3128 4.4476 11.8558 5.44528 12.0757 6.53137C12.2957 7.61745 12.1826 8.74317 11.7508 9.76618C11.3191 10.7892 10.588 11.6636 9.65009 12.2787C8.71217 12.8939 7.60949 13.2222 6.48148 13.2222C5.35347 13.2222 4.2508 12.8939 3.31287 12.2787C2.37494 11.6636 1.64388 10.7892 1.21213 9.76618C0.780373 8.74317 0.667312 7.61745 0.887239 6.53137C1.10717 5.44528 1.65021 4.4476 2.4477 3.66447M6.48465 0.777778V6.99909", 13.0f, 14.0f, area);
}

juce::Path gear (juce::Rectangle<float> area)
{
    auto path = fitted ("M9.97418 8.16032C9.9421 7.98338 9.96375 7.80088 10.0364 7.63636C10.1055 7.47504 10.2203 7.33745 10.3667 7.24053C10.513 7.14362 10.6845 7.09161 10.86 7.09091H10.9091C11.1984 7.09091 11.4759 6.97597 11.6805 6.77139C11.8851 6.5668 12 6.28933 12 6C12 5.71067 11.8851 5.4332 11.6805 5.22861C11.4759 5.02403 11.1984 4.90909 10.9091 4.90909H10.8164C10.6408 4.90839 10.4694 4.85638 10.323 4.75947C10.1767 4.66256 10.0619 4.52496 9.99273 4.36364V4.32C9.92012 4.15548 9.89846 3.97299 9.93054 3.79604C9.96263 3.6191 10.047 3.45583 10.1727 3.32727L10.2055 3.29455C10.3069 3.19323 10.3873 3.07291 10.4422 2.94048C10.4971 2.80805 10.5254 2.66609 10.5254 2.52273C10.5254 2.37936 10.4971 2.23741 10.4422 2.10497C10.3873 1.97254 10.3069 1.85223 10.2055 1.75091C10.1041 1.64948 9.98382 1.56902 9.85139 1.51412C9.71896 1.45922 9.577 1.43096 9.43364 1.43096C9.29027 1.43096 9.14832 1.45922 9.01588 1.51412C8.88345 1.56902 8.76313 1.64948 8.66182 1.75091L8.62909 1.78364C8.50054 1.90938 8.33726 1.99374 8.16032 2.02582C7.98338 2.0579 7.80088 2.03624 7.63636 1.96364C7.47504 1.89449 7.33745 1.77969 7.24053 1.63334C7.14362 1.487 7.09161 1.31552 7.09091 1.14V1.09091C7.09091 0.801582 6.97597 0.524105 6.77139 0.31952C6.5668 0.114935 6.28933 0 6 0C5.71067 0 5.4332 0.114935 5.22861 0.31952C5.02403 0.524105 4.90909 0.801582 4.90909 1.09091V1.18364C4.90839 1.35916 4.85638 1.53064 4.75947 1.67698C4.66256 1.82332 4.52496 1.93813 4.36364 2.00727H4.32C4.15548 2.07988 3.97299 2.10154 3.79604 2.06946C3.6191 2.03737 3.45583 1.95302 3.32727 1.82727L3.29455 1.79455C3.19323 1.69312 3.07291 1.61265 2.94048 1.55775C2.80805 1.50285 2.66609 1.4746 2.52273 1.4746C2.37936 1.4746 2.23741 1.50285 2.10497 1.55775C1.97254 1.61265 1.85223 1.69312 1.75091 1.79455C1.64948 1.89586 1.56902 2.01618 1.51412 2.14861C1.45922 2.28104 1.43096 2.423 1.43096 2.56636C1.43096 2.70973 1.45922 2.85168 1.51412 2.98412C1.56902 3.11655 1.64948 3.23687 1.75091 3.33818L1.78364 3.37091C1.90938 3.49946 1.99374 3.66274 2.02582 3.83968C2.0579 4.01662 2.03624 4.19912 1.96364 4.36364C1.9013 4.53313 1.78941 4.67995 1.64253 4.78501C1.49564 4.89006 1.32054 4.9485 1.14 4.95273H1.09091C0.801582 4.95273 0.524105 5.06766 0.31952 5.27225C0.114935 5.47683 0 5.75431 0 6.04364C0 6.33296 0.114935 6.61044 0.31952 6.81503C0.524105 7.01961 0.801582 7.13455 1.09091 7.13455H1.18364C1.35916 7.13525 1.53064 7.18725 1.67698 7.28417C1.82332 7.38108 1.93813 7.51867 2.00727 7.68C2.07988 7.84452 2.10154 8.02701 2.06946 8.20396C2.03737 8.3809 1.95302 8.54418 1.82727 8.67273L1.79455 8.70545C1.69312 8.80677 1.61265 8.92709 1.55775 9.05952C1.50285 9.19195 1.4746 9.33391 1.4746 9.47727C1.4746 9.62064 1.50285 9.76259 1.55775 9.89503C1.61265 10.0275 1.69312 10.1478 1.79455 10.2491C1.89586 10.3505 2.01618 10.431 2.14861 10.4859C2.28104 10.5408 2.423 10.569 2.56636 10.569C2.70973 10.569 2.85168 10.5408 2.98412 10.4859C3.11655 10.431 3.23687 10.3505 3.33818 10.2491L3.37091 10.2164C3.49946 10.0906 3.66274 10.0063 3.83968 9.97418C4.01662 9.9421 4.19912 9.96375 4.36364 10.0364C4.53313 10.0987 4.67995 10.2106 4.78501 10.3575C4.89006 10.5044 4.9485 10.6795 4.95273 10.86V10.9091C4.95273 11.1984 5.06766 11.4759 5.27225 11.6805C5.47683 11.8851 5.75431 12 6.04364 12C6.33296 12 6.61044 11.8851 6.81503 11.6805C7.01961 11.4759 7.13455 11.1984 7.13455 10.9091V10.8164C7.13525 10.6408 7.18725 10.4694 7.28417 10.323C7.38108 10.1767 7.51867 10.0619 7.68 9.99273C7.84452 9.92012 8.02701 9.89846 8.20396 9.93054C8.3809 9.96263 8.54418 10.047 8.67273 10.1727L8.70545 10.2055C8.80677 10.3069 8.92709 10.3873 9.05952 10.4422C9.19195 10.4971 9.33391 10.5254 9.47727 10.5254C9.62064 10.5254 9.76259 10.4971 9.89503 10.4422C10.0275 10.3873 10.1478 10.3069 10.2491 10.2055C10.3505 10.1041 10.431 9.98382 10.4859 9.85139C10.5408 9.71896 10.569 9.577 10.569 9.43364C10.569 9.29027 10.5408 9.14832 10.4859 9.01588C10.431 8.88345 10.3505 8.76313 10.2491 8.66182L10.2164 8.62909C10.0906 8.50054 10.0063 8.33726 9.97418 8.16032ZM7.63636 6C7.63636 6.90374 6.90374 7.63636 6 7.63636C5.09626 7.63636 4.36364 6.90374 4.36364 6C4.36364 5.09626 5.09626 4.36364 6 4.36364C6.90374 4.36364 7.63636 5.09626 7.63636 6Z", 12.0f, 12.0f, area);
    path.setUsingNonZeroWinding (false);
    return path;
}

juce::Path plus (juce::Rectangle<float> area)
{
    juce::Path path;
    path.startNewSubPath (area.getCentreX(), area.getY());
    path.lineTo (area.getCentreX(), area.getBottom());
    path.startNewSubPath (area.getX(), area.getCentreY());
    path.lineTo (area.getRight(), area.getCentreY());
    return path;
}

juce::Path chevrons (juce::Rectangle<float> area)
{
    const auto x = area.getCentreX();
    const auto halfWidth = area.getWidth() * 0.5f;
    const auto top = area.getY();
    const auto bottom = area.getBottom();
    const auto height = area.getHeight() * 0.36f;

    juce::Path path;
    path.startNewSubPath (x - halfWidth, top + height);
    path.lineTo (x, top);
    path.lineTo (x + halfWidth, top + height);
    path.startNewSubPath (x - halfWidth, bottom - height);
    path.lineTo (x, bottom);
    path.lineTo (x + halfWidth, bottom - height);
    return path;
}

} // namespace icons

} // namespace fancy::ui
