package com.raydose.raylink.ui.components

import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.DialogProperties
import com.raydose.raylink.ui.theme.RaylinkTextPrimary

@Composable
fun MessageEditDialog(
    initialText: String,
    isNew: Boolean,
    onDismiss: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    var text by remember(initialText) { mutableStateOf(initialText) }
    val dialogBackground = Color(0xFF3946A1)
    val dialogAccent = Color(0xFF8EA2FF)
    val inputBackground = Color(0xFF4452B8)
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = if (isNew) "新增留言" else "编辑留言",
                color = RaylinkTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            OutlinedTextField(
                value = text,
                onValueChange = { text = it },
                minLines = 3,
                maxLines = 5,
                label = { Text("留言内容", fontSize = 20.sp) },
                textStyle = androidx.compose.ui.text.TextStyle(
                    color = RaylinkTextPrimary,
                    fontSize = 22.sp,
                ),
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = RaylinkTextPrimary,
                    unfocusedTextColor = RaylinkTextPrimary,
                    focusedContainerColor = inputBackground,
                    unfocusedContainerColor = inputBackground,
                    cursorColor = dialogAccent,
                    focusedBorderColor = dialogAccent,
                    unfocusedBorderColor = Color(0xFF6F7CE0),
                    focusedLabelColor = dialogAccent,
                    unfocusedLabelColor = Color(0xFFD6DCFF),
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .defaultMinSize(minHeight = 160.dp),
            )
        },
        confirmButton = {
            TextButton(onClick = { onConfirm(text) }) {
                Text("保存", fontSize = 22.sp)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("取消", color = Color(0xFFD6DCFF), fontSize = 22.sp)
            }
        },
        containerColor = dialogBackground,
        modifier = Modifier.fillMaxWidth(0.66f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}
