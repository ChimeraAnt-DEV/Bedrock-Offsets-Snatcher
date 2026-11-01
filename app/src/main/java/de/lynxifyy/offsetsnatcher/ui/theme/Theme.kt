package de.lynxifyy.offsetsnatcher.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val LightColors = lightColorScheme(
    primary = BerryBlue,
    onPrimary = Color.White,
    primaryContainer = BerryBlueLight,
    onPrimaryContainer = BerryBlueDark,
    secondary = BerryOrange,
    onSecondary = InkBlack,
    secondaryContainer = BerryOrangeLight,
    onSecondaryContainer = InkBlackSoft,
    background = WarmWhite,
    onBackground = InkBlack,
    surface = SurfaceWhite,
    onSurface = InkBlack,
    surfaceVariant = BerryBlueLight,
    onSurfaceVariant = InkBlackSoft,
    outline = BerryBlue.copy(alpha = 0.4f),
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFF7EADFF),
    onPrimary = NightBlack,
    primaryContainer = Color(0xFF21375F),
    onPrimaryContainer = Color(0xFFD9E6FF),
    secondary = BerryOrange,
    onSecondary = NightBlack,
    secondaryContainer = Color(0xFF4A2B00),
    onSecondaryContainer = BerryOrangeLight,
    background = NightBlack,
    onBackground = Color(0xFFE8ECF1),
    surface = NightSurface,
    onSurface = Color(0xFFE8ECF1),
    surfaceVariant = NightSurfaceHigh,
    onSurfaceVariant = Color(0xFFB9C2CC),
    outline = Color(0xFF5A6874),
)

@Composable
fun GlowBerryTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = if (darkTheme) DarkColors else LightColors,
        typography = Typography,
        content = content,
    )
}