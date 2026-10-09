using System.Drawing;
using System.Windows.Forms;

namespace KharvoxLauncher;

// UI prompts for resource collisions. The patch writer never makes a silent
// priority decision: the caller must supply an explicit selection callback.
internal static class KharvoxResourceConflictPrompts
{
    internal static bool ApproveSimpleMerge(Form owner,
        KharvoxResourcePatcher.MergeProposal proposal)
    {
        if (owner.IsDisposed) return false;
        if (owner.InvokeRequired)
            return (bool)owner.Invoke(new Func<bool>(
                () => ApproveSimpleMerge(owner, proposal)));

        var message =
            "Two enabled DOOM mods change the same configuration file."
            + Environment.NewLine + Environment.NewLine
            + "File: " + proposal.ResourcePath
            + Environment.NewLine + "Mod A: " + Path.GetFileName(proposal.FirstMod)
            + Environment.NewLine + "Mod B: " + Path.GetFileName(proposal.SecondMod)
            + Environment.NewLine + Environment.NewLine
            + "KHARVOX found a straightforward merge of different settings."
            + Environment.NewLine + Environment.NewLine
            + "WARNING: A valid merge can still cause unexpected gameplay issues."
            + " This is not a guarantee that the mods are compatible."
            + Environment.NewLine + Environment.NewLine
            + "Attempt this merge?"
            + Environment.NewLine + "YES: Use the merged result."
            + Environment.NewLine + "NO: Cancel this patch build and leave existing files unchanged.";

        return MessageBox.Show(owner, message, "KHARVOX — Mod merge warning",
            MessageBoxButtons.YesNo, MessageBoxIcon.Warning,
            MessageBoxDefaultButton.Button2) == DialogResult.Yes;
    }

    internal static KharvoxResourcePatcher.ConflictChoice ChooseWinner(Form owner,
        KharvoxResourcePatcher.ConflictProposal proposal)
    {
        if (owner.IsDisposed)
            return KharvoxResourcePatcher.ConflictChoice.Cancel;
        if (owner.InvokeRequired)
            return (KharvoxResourcePatcher.ConflictChoice)owner.Invoke(
                new Func<KharvoxResourcePatcher.ConflictChoice>(
                    () => ChooseWinner(owner, proposal)));

        var choice = KharvoxResourcePatcher.ConflictChoice.Cancel;
        using (var dialog = new Form
        {
            Text = "KHARVOX — DOOM mod conflict",
            StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            ClientSize = new Size(590, 310),
            MinimizeBox = false,
            MaximizeBox = false,
            ShowInTaskbar = false,
            BackColor = Color.FromArgb(27, 27, 30),
            ForeColor = Color.White
        })
        {
            var explanation = new Label
            {
                Bounds = new Rectangle(20, 16, 550, 184),
                Text = "Both mods modify the same DOOM resource."
                     + Environment.NewLine + Environment.NewLine
                     + "Resource: " + proposal.ResourcePath
                     + Environment.NewLine + "Mod A: " + Path.GetFileName(proposal.FirstMod)
                     + Environment.NewLine + "Mod B: " + Path.GetFileName(proposal.SecondMod)
                     + Environment.NewLine + "Reason: " + proposal.Reason
                     + Environment.NewLine + Environment.NewLine
                     + "KHARVOX cannot reliably merge these modifications."
                     + " Choose which mod's version of this file to use.",
                Font = new Font("Segoe UI", 9.5F)
            };
            dialog.Controls.Add(explanation);
            var caution = new Label
            {
                Bounds = new Rectangle(20, 209, 550, 38),
                Text = "Other files from both mods remain enabled. This choice may"
                    + " affect gameplay. Cancel makes no changes.",
                ForeColor = Color.Gold
            };
            dialog.Controls.Add(caution);

            Button MakeButton(string title, int x, int width,
                KharvoxResourcePatcher.ConflictChoice option)
            {
                var button = new Button
                {
                    Text = title, Bounds = new Rectangle(x, 258, width, 34),
                    FlatStyle = FlatStyle.Flat,
                    BackColor = Color.FromArgb(48, 48, 52),
                    ForeColor = Color.White
                };
                button.Click += (_, _) =>
                {
                    choice = option;
                    dialog.DialogResult = option ==
                        KharvoxResourcePatcher.ConflictChoice.Cancel
                        ? DialogResult.Cancel : DialogResult.OK;
                    dialog.Close();
                };
                dialog.Controls.Add(button);
                return button;
            }
            MakeButton("Use Mod A", 20, 170,
                KharvoxResourcePatcher.ConflictChoice.UseFirst);
            MakeButton("Use Mod B", 202, 170,
                KharvoxResourcePatcher.ConflictChoice.UseSecond);
            var cancel = MakeButton("Cancel", 388, 182,
                KharvoxResourcePatcher.ConflictChoice.Cancel);
            dialog.CancelButton = cancel;
            dialog.ShowDialog(owner);
        }
        return choice;
    }
}
