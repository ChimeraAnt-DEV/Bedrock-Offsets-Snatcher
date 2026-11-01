package de.lynxifyy.offsetsnatcher.ui.screens

import android.content.Context
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Error
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import de.lynxifyy.offsetsnatcher.ExtractPhase
import de.lynxifyy.offsetsnatcher.MainViewModel
import de.lynxifyy.offsetsnatcher.PipelineState
import de.lynxifyy.offsetsnatcher.ui.theme.BerryBlue
import de.lynxifyy.offsetsnatcher.ui.theme.BerryOrange

/** Home screen: pick APK, watch progress, copy/export the JSON. */
@Composable
fun MainScreen(
    viewModel: MainViewModel,
    onPickApk: () -> Unit,
) {
    val context = LocalContext.current
    val state by viewModel.state.collectAsState()

    val exportLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/json")
    ) { uri: Uri? ->
        if (uri != null) writeToUri(context, uri, state.jsonOutput ?: return@rememberLauncherForActivityResult)
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Spacer(Modifier.height(12.dp))
        LogoHeader()
        Spacer(Modifier.height(16.dp))
        PickCard(onPick = { onPickApk() }, pickedName = null)
        Spacer(Modifier.height(16.dp))
        PipelineCard(state)
        Spacer(Modifier.height(16.dp))
        ResultCard(state, exportLauncher::launch, viewModel::reset)
    }
}

@Composable
private fun LogoHeader() {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(
            modifier = Modifier
                .size(76.dp)
                .clip(RoundedCornerShape(22.dp))
                .background(
                    Brush.linearGradient(
                        listOf(BerryBlue, BerryBlue.copy(alpha = 0.7f)),
                    )
                ),
            contentAlignment = Alignment.Center,
        ) {
            Icon(
                imageVector = Icons.Filled.Bolt,
                contentDescription = null,
                tint = Color.White,
                modifier = Modifier.size(42.dp),
            )
        }
        Spacer(Modifier.height(14.dp))
        Text(
            text = "GlowBerry",
            style = MaterialTheme.typography.displaySmall,
            color = MaterialTheme.colorScheme.onBackground,
        )
        Text(
            text = "Offset Snatcher",
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.primary,
        )
        Spacer(Modifier.height(6.dp))
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Icon(
                imageVector = Icons.Filled.Shield,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.secondary,
                modifier = Modifier.size(14.dp),
            )
            Text(
                text = "100% on-device · nothing leaves your phone",
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun PickCard(onPick: () -> Unit, pickedName: String?) {
    Card(
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface,
        ),
        shape = RoundedCornerShape(22.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 3.dp),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                text = "Pick your Minecraft APK",
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(6.dp))
            Text(
                text = "arm64-v8a Bedrock Edition (*.apk)",
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(16.dp))
            Button(
                onClick = onPick,
                shape = RoundedCornerShape(50),
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.primary,
                    contentColor = MaterialTheme.colorScheme.onPrimary,
                ),
            ) {
                Icon(
                    Icons.Filled.FolderOpen,
                    contentDescription = null,
                    modifier = Modifier.size(18.dp),
                )
                Spacer(Modifier.size(8.dp))
                Text("Choose APK")
            }
        }
    }
}

@Composable
private fun PipelineCard(state: PipelineState) {
    val isWorking =
        state.phase == ExtractPhase.EXTRACTING ||
            state.phase == ExtractPhase.PARSING ||
            state.phase == ExtractPhase.SCANNING

    val indicatorColor = when (state.phase) {
        ExtractPhase.ERROR -> MaterialTheme.colorScheme.error
        ExtractPhase.DONE -> MaterialTheme.colorScheme.secondary
        else -> MaterialTheme.colorScheme.primary
    }
    val progress by animateFloatAsState(
        targetValue = state.progress,
        animationSpec = spring(dampingRatio = Spring.DampingRatioNoBouncy),
    )

    Card(
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant,
        ),
        shape = RoundedCornerShape(22.dp),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                if (isWorking) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(20.dp),
                        strokeWidth = 2.dp,
                    )
                } else if (state.phase == ExtractPhase.DONE) {
                    Icon(
                        imageVector = Icons.Filled.Check,
                        tint = MaterialTheme.colorScheme.secondary,
                        contentDescription = null,
                        modifier = Modifier.size(22.dp),
                    )
                } else if (state.phase == ExtractPhase.ERROR) {
                    Icon(
                        imageVector = Icons.Filled.Error,
                        tint = MaterialTheme.colorScheme.error,
                        contentDescription = null,
                        modifier = Modifier.size(22.dp),
                    )
                }
                Text(
                    text = state.phase.label,
                    style = MaterialTheme.typography.titleMedium,
                    color = MaterialTheme.colorScheme.onSurface,
                )
            }
            Spacer(Modifier.height(12.dp))
            LinearProgressIndicator(
                progress = { progress },
                modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(50)),
                color = indicatorColor,
                trackColor = MaterialTheme.colorScheme.background,
            )
            Spacer(Modifier.height(10.dp))
            Text(
                text = state.message.ifEmpty { "Waiting for an APK…" },
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = TextAlign.Center,
            )
        }
    }
}

@Composable
private fun ResultCard(
    state: PipelineState,
    onExport: (String) -> Unit,
    onReset: () -> Unit,
) {
    val context = LocalContext.current
    val clipboard = LocalClipboardManager.current
    val json = state.jsonOutput
    var showAll by remember { mutableStateOf(false) }

    AnimatedVisibility(visible = json != null) {
        Card(
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surface,
            ),
            shape = RoundedCornerShape(22.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 3.dp),
        ) {
            Column(
                modifier = Modifier.fillMaxWidth().padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(
                    text = "BerryAPI Offsets",
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    text = "${json?.count { it == '\n' }?.plus(1) ?: 0} lines",
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(12.dp))

                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(260.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(MaterialTheme.colorScheme.background),
                ) {
                    Text(
                        text = previewJson(json ?: "", showAll),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurface,
                        modifier = Modifier
                            .fillMaxSize()
                            .verticalScroll(rememberScrollState())
                            .padding(12.dp),
                    )
                }
                Spacer(Modifier.height(8.dp))
                TextButton(onClick = { showAll = !showAll }) {
                    Text(if (showAll) "Show less" else "Show full JSON")
                }

                Spacer(Modifier.height(10.dp))
                Row(
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Button(
                        onClick = {
                            clipboard.setText(androidx.compose.ui.text.AnnotatedString(json ?: ""))
                            Toast.makeText(context, "JSON copied", Toast.LENGTH_SHORT).show()
                        },
                        shape = RoundedCornerShape(50),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = BerryOrange,
                            contentColor = Color.White,
                        ),
                    ) {
                        Icon(Icons.Filled.ContentCopy, null, Modifier.size(16.dp))
                        Spacer(Modifier.size(6.dp))
                        Text("Copy")
                    }
                    Button(
                        onClick = { json?.let(onExport) },
                        shape = RoundedCornerShape(50),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = MaterialTheme.colorScheme.primary,
                            contentColor = MaterialTheme.colorScheme.onPrimary,
                        ),
                    ) {
                        Icon(Icons.Filled.Save, null, Modifier.size(16.dp))
                        Spacer(Modifier.size(6.dp))
                        Text("Export .json")
                    }
                }
                Spacer(Modifier.height(8.dp))
                TextButton(onClick = onReset) {
                    Icon(Icons.Filled.Refresh, null, Modifier.size(14.dp))
                    Spacer(Modifier.size(4.dp))
                    Text("Start over")
                }
            }
        }
    }
}

private fun previewJson(json: String, showAll: Boolean): String {
    // Keep the preview readable while honouring "show full JSON".
    if (showAll) return json
    val lines = json.lineSequence().toList()
    val cap = 40
    if (lines.size <= cap) return json
    return lines.take(cap).joinToString("\n") + "\n…"
}

private fun writeToUri(context: Context, uri: Uri, json: String) {
    try {
        context.contentResolver.openOutputStream(uri)?.use { it.write(json.toByteArray()) }
            ?: Toast.makeText(context, "Could not open destination", Toast.LENGTH_SHORT).show()
    } catch (e: Exception) {
        Toast.makeText(context, "Export failed: ${e.message}", Toast.LENGTH_SHORT).show()
    }
}