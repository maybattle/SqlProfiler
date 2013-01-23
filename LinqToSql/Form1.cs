using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;

using System.Windows.Forms;

namespace LinqToSql
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            var ctx = new DataClasses1DataContext();
            var result = ctx.JWEwolf_Debitors.Where(n => n.intDebitorId == 1 && n.dtmModifyDate <= DateTime.Now && n.strCreateUser.Contains("jack")).ToList();

        }
    }
}
